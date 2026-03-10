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
int           MOTOR_CH     = -1;

bool         actionLock     = false;
unsigned long limitOpenSince  = 0;
unsigned long limitCloseSince = 0;

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

    MOTOR_CH = ledcAttach(MOTOR_ENA, 2000, 8); // 2 kHz, 8 Bit
    if (MOTOR_CH < 0)
        Serial.println("❌ LEDC: Kein Kanal verfügbar");
    else
        Serial.printf("✅ LEDC Kanal %d zugewiesen\n", MOTOR_CH);
}

// ==================================================
// HARDWARE-PRIMITIVES
// ==================================================
void motorStop()
{
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    if (MOTOR_CH >= 0) ledcWrite(MOTOR_CH, 0);
}

void motorOpen()
{
    digitalWrite(MOTOR_IN1, HIGH);
    digitalWrite(MOTOR_IN2, LOW);
    if (MOTOR_CH >= 0) ledcWrite(MOTOR_CH, 180);
}

void motorClose()
{
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, HIGH);
    if (MOTOR_CH >= 0) ledcWrite(MOTOR_CH, 180);
}

void startMotorOpen(unsigned long durationMs)
{
    motorOpen();
    motorState = MOTOR_OPENING;
    motorUntil = millis() + durationMs;
}

void startMotorClose(unsigned long durationMs)
{
    motorClose();
    motorState = MOTOR_CLOSING;
    motorUntil = millis() + durationMs;
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
