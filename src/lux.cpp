#include "lux.h"
#include "logger.h"
#include "pins.h"
#include <Adafruit_VEML7700.h>
#include <Wire.h>
#include <math.h>

// ==================================================
// GLOBALE VARIABLEN (dieses Moduls)
// ==================================================
float lux  = 0.0f;
bool  hasVEML      = false;
bool  vemlHardError  = false;
bool  vemlSoftError  = false;
float luxTrend       = 0.0f;
float luxRateFiltered = 0.0f;
bool  luxReady       = false;

// FIX: auf millis() initialisieren statt 0.
// Mit 0 wäre (nowMs - vemlLastLux) nach 5 Minuten sofort > LUX_FALLBACK_AFTER_MS,
// selbst wenn der Sensor einwandfrei funktioniert.
unsigned long vemlLastLux          = 0;  // wird in luxInit() auf millis() gesetzt
bool          lightAutomationAvailable = false;

float         lastLux  = 0.0f;
unsigned long lastLuxTime = 0;

bool          forecastTestMode  = false;
float         testLuxStart      = 200.0f;
float         testLuxEnd        = 5.0f;
float         testDurationMin   = 20.0f;
unsigned long testStartMillis   = 0;

// ==================================================
// INTERN
// ==================================================
static Adafruit_VEML7700 veml;

static uint8_t vemlGain = VEML7700_GAIN_1_8;
static uint8_t vemlIT   = VEML7700_IT_25MS;

static uint8_t       luxFailCount   = 0;
static unsigned long luxInvalidSince = 0;
static unsigned long lastVemlReinit  = 0;

static float luxHistory[20]      = {0};
static uint8_t luxHistoryIdx     = 0;

static float luxMedianBuf[5]     = {0};
static uint8_t luxMedianIdx      = 0;

const unsigned long VEML_REINIT_COOLDOWN = 15000UL;
const unsigned long VEML_HARD_TIMEOUT    = 60000UL;
const unsigned long LUX_FALLBACK_AFTER_MS = 5UL * 60UL * 1000UL;

// ==================================================
bool luxInit()
{
    // FIX: vemlLastLux auf jetzt setzen damit der 5-Minuten-Fallback-Timer
    // ab Boot läuft, nicht ab Zeitstempel 0.
    vemlLastLux = millis();

    hasVEML = veml.begin();
    if (hasVEML)
    {
        veml.setGain(VEML7700_GAIN_1_8);
        veml.setIntegrationTime(VEML7700_IT_25MS);
        Serial.println("✅ VEML7700 initialisiert");
    }
    else
    {
        Serial.println("⚠️ VEML7700 nicht gefunden");
    }
    return hasVEML;
}

float getLux()
{
    if (!hasVEML) return NAN;
    float val = veml.readLux();
    if (!isfinite(val) || val < 0 || val > 120000) return NAN;
    autoRangeVEML(val);
    return val;
}

float medianLux(float newValue)
{
    luxMedianBuf[luxMedianIdx] = newValue;
    luxMedianIdx = (luxMedianIdx + 1) % 5;

    float tmp[5];
    memcpy(tmp, luxMedianBuf, sizeof(tmp));
    // einfacher Bubble-Sort
    for (int i = 0; i < 4; i++)
        for (int j = i + 1; j < 5; j++)
            if (tmp[j] < tmp[i]) { float t = tmp[i]; tmp[i] = tmp[j]; tmp[j] = t; }
    return tmp[2];
}

void updateLuxTrend(float currentLux)
{
    luxHistory[luxHistoryIdx] = currentLux;
    luxHistoryIdx = (luxHistoryIdx + 1) % 20;
    float oldest = luxHistory[luxHistoryIdx];
    if (oldest == 0) return;
    luxTrend = currentLux - oldest;
}

// ==================================================
// FEHLERÜBERWACHUNG
// ==================================================
void checkLuxHealth(unsigned long nowMs, float rawLux, bool luxValid)
{
    if (!luxValid)
    {
        if (luxInvalidSince == 0) luxInvalidSince = nowMs;
        if (luxFailCount < 255)   luxFailCount++;
    }
    else
    {
        luxFailCount    = 0;
        luxInvalidSince = 0;
        vemlSoftError   = false;
        vemlHardError   = false;
        vemlLastLux     = nowMs;
    }

    // Soft-Error
    if (hasVEML && luxFailCount >= 3 && !vemlSoftError)
        vemlSoftError = true;

    // Hard-Error: 60 Sekunden keine gültigen Daten
    if (hasVEML && luxInvalidSince > 0 &&
        nowMs - luxInvalidSince > VEML_HARD_TIMEOUT && !vemlHardError)
    {
        vemlHardError = true;
        hasVEML       = false;
        Serial.println("❌ VEML Hard-Error");
        addLog("VEML7700 Hard-Error");
    }

    // Periodischer Reinit-Versuch solange Sensor ausgefallen.
    // Läuft alle VEML_REINIT_COOLDOWN (15s) – ermöglicht Selbstheilung
    // nach I2C-Hänger, Spannungseinbruch oder kurzzeitiger Störung.
    if (!hasVEML && nowMs - lastVemlReinit > VEML_REINIT_COOLDOWN)
        reinitVEML7700();

    // Verfügbarkeit für Lichtautomatik
    lightAutomationAvailable = hasVEML && !vemlHardError &&
                               (nowMs - vemlLastLux) < LUX_FALLBACK_AFTER_MS;
}

// ==================================================
// I2C RECOVERY
// ==================================================
void i2cBusRecover()
{
    Serial.println("⚠️ I2C BUS RECOVERY START");
    pinMode(I2C_SDA, INPUT_PULLUP);
    pinMode(I2C_SCL, INPUT_PULLUP);
    delay(10);

    pinMode(I2C_SCL, OUTPUT);
    for (int i = 0; i < 9; i++)
    {
        digitalWrite(I2C_SCL, HIGH); delayMicroseconds(5);
        digitalWrite(I2C_SCL, LOW);  delayMicroseconds(5);
    }
    pinMode(I2C_SDA, OUTPUT);
    digitalWrite(I2C_SDA, LOW);  delayMicroseconds(5);
    digitalWrite(I2C_SCL, HIGH); delayMicroseconds(5);
    digitalWrite(I2C_SDA, HIGH);
    delay(10);

    Wire.end();
    delay(20);
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(30000);
    Wire.setTimeOut(50);
    Serial.println("✅ I2C BUS RECOVERY DONE");
}

// ==================================================
// REINIT
// ==================================================
void reinitVEML7700()
{
    if (millis() - lastVemlReinit < VEML_REINIT_COOLDOWN) return;
    lastVemlReinit = millis();

    Serial.println("🔄 VEML Reinit...");
    i2cBusRecover();

    if (veml.begin())
    {
        veml.setGain(VEML7700_GAIN_1_8);
        veml.setIntegrationTime(VEML7700_IT_25MS);
        vemlGain = VEML7700_GAIN_1_8;
        vemlIT   = VEML7700_IT_25MS;

        hasVEML     = true;
        luxFailCount = 0;
        vemlSoftError = false;
        vemlHardError = false;

        Serial.println("✅ VEML reinitialisiert");
        addLog("VEML7700 reinitialisiert");
    }
    else
    {
        hasVEML = false;
        Serial.println("❌ VEML Reinit fehlgeschlagen");
    }
}

// ==================================================
// AUTO-RANGE
// ==================================================
void autoRangeVEML(float luxValue)
{
    if (luxValue > 20000 && vemlGain != VEML7700_GAIN_1_8)
    {
        vemlGain = VEML7700_GAIN_1_8;
        veml.setGain(vemlGain);
    }
    else if (luxValue < 10 && vemlGain != VEML7700_GAIN_2)
    {
        vemlGain = VEML7700_GAIN_2;
        veml.setGain(vemlGain);
    }
    else if (luxValue >= 10 && luxValue <= 20000 && vemlGain != VEML7700_GAIN_1)
    {
        vemlGain = VEML7700_GAIN_1;
        veml.setGain(vemlGain);
    }
}
