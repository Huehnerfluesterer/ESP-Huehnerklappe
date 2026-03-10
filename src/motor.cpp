#include "motor.h"
#include "light.h"    // lightState, startLightForMinutesReset, dimmingActive, ...
#include "door.h"     // doorOpen, doorPhase, doorOpenedAt
#include "lux.h"      // lux
#include "storage.h"  // lampPostOpen, lampPostClose, openMode, closeMode
#include "logger.h"
#include "pins.h"
#include <Arduino.h>

// ==================================================
// GLOBALE VARIABLEN (dieses Moduls)
// ==================================================
MotorState    motorState   = MOTOR_STOPPED;
unsigned long motorUntil   = 0;
String        motorReason  = "";

bool         actionLock     = false;
unsigned long limitOpenSince  = 0;
unsigned long limitCloseSince = 0;

// --- ACS712 Blockadeerkennung ---
float         currentBaseline    = 0.0f;  // Leerlauf-Strom beim Motorstart (A)
bool          currentCalibrated  = false; // Baseline eingemessen?
unsigned long motorStartedAt     = 0;     // Zeitpunkt Motorstart für Settle-Zeit
bool          blockadeEnabled    = true;  // wird aus EEPROM geladen
float         blockadeThresholdA = BLOCKADE_THRESHOLD_A; // wird aus EEPROM geladen
float         peakCurrentA       = 0.0f;  // höchster gemessener Strom seit letztem Reset

// Spannungsteiler-Korrektur: 10kΩ/20kΩ → Faktor 20/(10+20) = 0.6667
// ADC-Bereich 0–4095 entspricht 0–3.3V
// Rückrechnung auf Sensor-Ausgangsspannung: Vmeas / 0.6667
// Strom = (Vsensor - ACS712_ZERO_V) / (ACS712_MV_PER_A / 1000.0)
static float measureCurrentAmps()
{
    // ESP32 ADC ist rauschbehaftet → 100 Samples mitteln
    const int SAMPLES = 100;
    long sum = 0;
    for (int i = 0; i < SAMPLES; i++) sum += analogRead(ACS712_PIN);
    float adcAvg  = (float)sum / SAMPLES;
    float vMeas   = adcAvg * (3.3f / 4095.0f);
    // Spannungsteiler-Korrektur nur wenn verbaut (ACS712_HAS_DIVIDER = 1)
#if ACS712_HAS_DIVIDER
    float vSensor = vMeas / (20.0f / 30.0f);  // 10k/20k Teiler zurückrechnen
#else
    float vSensor = vMeas;                     // kein Teiler: direkt messen
#endif
    float amps    = fabsf((vSensor - ACS712_ZERO_V) / (ACS712_MV_PER_A / 1000.0f));
    if (amps > 8.0f) return 0.0f;  // Pin floatet / kein Sensor
    return amps;
}

static void calibrateBaseline()
{
    // 5 schnelle Messungen ohne delay()
    float sum = 0.0f;
    for (int i = 0; i < 5; i++) sum += measureCurrentAmps();
    currentBaseline   = sum / 5.0f;
    currentCalibrated = true;
    Serial.printf("⚡ ACS712 Baseline: %.2f A  (Blockade ab %.2f A)\n",
                  currentBaseline, currentBaseline + blockadeThresholdA);
}

const unsigned long LIMIT_DEBOUNCE_MS = 40;

// Vorwärts-Deklarationen aus anderen Modulen
void saveDoorState();
void addLogWithLux(const String &text, float lx);

// Interne Variablen aus logic.h (Prognose-Zustand zurücksetzen nach Öffnen/Schließen)
extern bool         preLightForecastActive;
extern bool         preLightForecastCondition;
extern unsigned long preLightStartedAt;
extern bool         preLightOpenDone;
extern bool         preLightCloseDone;
extern unsigned long lightBelowSince;
extern unsigned long closeInterruptionSince;
extern unsigned long plannedCloseAt;
extern bool         closeForecastCondition;
extern unsigned long closeForecastStableSince;
extern unsigned long lastDoorCloseTime;
extern unsigned long scheduledCloseAt;
extern bool          nightLock;

// ==================================================
void motorInit()
{
    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);

    ledcAttach(MOTOR_ENA, 2000, 8); // 2 kHz, 8 Bit – Pin-basiert (Core 3.x)
    Serial.println("✅ LEDC Motor-PWM initialisiert (GPIO " + String(MOTOR_ENA) + ")");
}

// ==================================================
// HARDWARE-PRIMITIVES
// ==================================================
void motorStop()
{
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    ledcWrite(MOTOR_ENA, 0);
}

void motorOpen()
{
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);
    ledcWrite(MOTOR_ENA, 180);
}

void motorClose()
{
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, HIGH);
    ledcWrite(MOTOR_ENA, 180);
}

void startMotorOpen(unsigned long durationMs)
{
    motorOpen();
    motorState        = MOTOR_OPENING;
    motorUntil        = millis() + durationMs;
    // Blockadeerkennung: Kalibrierung nach Anlaufzeit
    currentCalibrated = false;
    motorStartedAt    = millis();
}

void startMotorClose(unsigned long durationMs)
{
    motorClose();
    motorState        = MOTOR_CLOSING;
    motorUntil        = millis() + durationMs;
    // Blockadeerkennung: Kalibrierung nach Anlaufzeit
    currentCalibrated = false;
    motorStartedAt    = millis();
}

// ==================================================
bool isManualAction()
{
    return motorReason.indexOf("manuell") >= 0 ||
           motorReason.indexOf("Taster")  >= 0 ||
           motorReason.indexOf("Web")     >= 0;
}

void reverseAfterBlockade()
{
    Serial.println("↩️ Rückwärtsfahren nach Blockade");
    lightState = LIGHT_POST_OPEN;
    startLightForMinutes(lampPostOpen);
    motorOpen();
    motorState = MOTOR_OPENING;
    motorUntil = millis() + 800;
}

// ==================================================
// UPDATE (zyklisch in loop())
// ==================================================
void updateMotor()
{
    if (motorState == MOTOR_STOPPED) return;

    // ===== STROMMESSUNG (immer, unabhängig von Blockadeerkennung) =====
    // Peak-Tracking läuft immer mit sobald Motor läuft
    static unsigned long lastCurrentCheck = 0;
    if (millis() - motorStartedAt > 200UL && millis() - lastCurrentCheck > 200UL)
    {
        lastCurrentCheck = millis();
        float ampsNow = measureCurrentAmps();
        if (ampsNow > peakCurrentA) peakCurrentA = ampsNow;
    }

    // ===== BLOCKADEERKENNUNG (ACS712) =====
    // Nach 500ms Anlaufzeit: Baseline einmessen; danach alle 200ms prüfen
    static unsigned long lastBlockadeCheck = 0;
    if (blockadeEnabled && millis() - motorStartedAt > 500UL)
    {
        if (!currentCalibrated)
        {
            calibrateBaseline();
        }
        else if (millis() - lastBlockadeCheck > 200UL)
        {
            lastBlockadeCheck = millis();
            float amps = measureCurrentAmps();
            if (!isnan(blockadeThresholdA) && amps > currentBaseline + blockadeThresholdA)
            {
                Serial.printf("🚨 Blockade! %.2f A (Baseline %.2f A)\n",
                              amps, currentBaseline);
                addLog(String("Blockade erkannt (") + String(amps, 1) + "A)");
                motorStop();
                motorState        = MOTOR_STOPPED;
                currentCalibrated = false;
                reverseAfterBlockade();
                return;
            }
        }
    }

    // ===== ENDSCHALTER ÖFFNEN =====
    if (useLimitSwitches && motorState == MOTOR_OPENING)
    {
        if (digitalRead(LIMIT_OPEN_PIN) == LOW)
        {
            if (limitOpenSince == 0) limitOpenSince = millis();
            if (millis() - limitOpenSince > LIMIT_DEBOUNCE_MS)
            {
                motorStop();
                doorOpen     = true;
                doorPhase    = PHASE_OPEN;
                actionLock   = false;
                doorOpenedAt = millis();
                saveDoorState();
                addLog("Endschalter OBEN erreicht");
                motorState    = MOTOR_STOPPED;
                limitOpenSince = 0;

                // Post-Licht
                if (!isManualAction())
                {
                    lightState = LIGHT_POST_OPEN;
                    startLightForMinutesReset(lampPostOpen);
                    addLogWithLux("Locklicht nach Öffnung gestartet", lux);
                }
                preLightForecastActive    = false;
                preLightForecastCondition = false;
                preLightStartedAt         = 0;
                lightBelowSince           = 0;
                closeInterruptionSince    = 0;
                plannedCloseAt            = 0;
                motorReason               = "";
                return;
            }
        }
        else { limitOpenSince = 0; }
    }

    // ===== ENDSCHALTER SCHLIESSEN =====
    if (useLimitSwitches && motorState == MOTOR_CLOSING)
    {
        if (digitalRead(LIMIT_CLOSE_PIN) == LOW)
        {
            if (limitCloseSince == 0) limitCloseSince = millis();
            if (millis() - limitCloseSince > LIMIT_DEBOUNCE_MS)
            {
                motorStop();
                doorOpen   = false;
                doorPhase  = PHASE_IDLE;
                actionLock = false;
                saveDoorState();
                addLog("Endschalter UNTEN erreicht");
                motorState     = MOTOR_STOPPED;
                limitCloseSince = 0;

                // Nacht-Sperre + Post-Licht
                lastDoorCloseTime      = millis();
                preLightOpenDone       = false;
                preLightCloseDone      = true;
                scheduledCloseAt       = 0;
                closeForecastCondition = false;
                closeForecastStableSince = 0;
                addLogWithLux("Klappe geschlossen (Endschalter)", lux);

                if (openMode == "light" && closeMode == "light")
                {
                    nightLock = true;
                    addLog("Nacht-Sperre aktiv");
                }
                if (!isManualAction())
                {
                    lightState = LIGHT_POST_CLOSE;
                    startLightForMinutesReset(lampPostClose);
                    lightActive = false;
                    addLogWithLux("Locklicht nach Schließen gestartet", lux);
                    if (lampPostClose >= 5)
                    {
                        dimEndTime    = lightStateUntil;
                        dimStartTime  = dimEndTime - 5UL * 60000UL;
                        dimmingActive = true;
                    }
                    else dimmingActive = false;
                }
                motorReason = "";
                return;
            }
        }
        else { limitCloseSince = 0; }
    }

    // ===== TIMEOUT =====
    if (millis() >= motorUntil)
    {
        motorStop();

        if (motorState == MOTOR_OPENING)
        {
            doorOpen     = true;
            doorPhase    = PHASE_OPEN;
            actionLock   = false;
            doorOpenedAt = millis();
            saveDoorState();

            if (motorReason.indexOf("Licht") >= 0)
                addLogWithLux("Klappe geöffnet (" + motorReason + ")", lux);
            else
                addLog("Klappe geöffnet (" + motorReason + ")");

            preLightForecastActive    = false;
            preLightForecastCondition = false;
            preLightStartedAt         = 0;

            if (!isManualAction())
            {
                lightState = LIGHT_POST_OPEN;
                startLightForMinutesReset(lampPostOpen);
                addLogWithLux("Locklicht nach Öffnung gestartet", lux);
            }
            lightBelowSince        = 0;
            closeInterruptionSince = 0;
            plannedCloseAt         = 0;
            motorReason            = "";
        }
        else if (motorState == MOTOR_CLOSING)
        {
            doorOpen   = false;
            doorPhase  = PHASE_IDLE;
            actionLock = false;
            saveDoorState();

            lightBelowSince          = 0;
            preLightOpenDone         = false;
            preLightCloseDone        = true;
            scheduledCloseAt         = 0;
            closeForecastCondition   = false;
            closeForecastStableSince = 0;
            lastDoorCloseTime        = millis();

            addLogWithLux("Klappe geschlossen (" + motorReason + ")", lux);

            if (openMode == "light" && closeMode == "light")
            {
                nightLock = true;
                addLog("Nacht-Sperre aktiv (durch Abschluss Schließen)");
            }
            if (!isManualAction())
            {
                lightState = LIGHT_POST_CLOSE;
                startLightForMinutesReset(lampPostClose);
                lightActive = false;
                addLogWithLux("Locklicht nach Schließen gestartet", lux);
                if (lampPostClose >= 5)
                {
                    dimEndTime    = lightStateUntil;
                    dimStartTime  = dimEndTime - 5UL * 60000UL;
                    dimmingActive = true;
                }
                else dimmingActive = false;
            }
            motorReason = "";
        }
        motorState = MOTOR_STOPPED;
    }
}
