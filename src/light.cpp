#include "light.h"
#include "logger.h"
#include "storage.h"
#include "pins.h"
#include <Arduino.h>

// ==================================================
// GLOBALE VARIABLEN
// ==================================================
LightState    lightState       = LIGHT_OFF;
unsigned long lightStateUntil  = 0;
bool          lightActive      = false;
bool          manualLightActive = false;

bool          stallLightActive  = false;
unsigned long stallLightOffTime = 0;

bool          rgbRedActive      = false;

bool          dimmingActive     = false;
unsigned long dimStartTime      = 0;
unsigned long dimEndTime        = 0;

// LEDC-Kanal-Handles (automatisch vergeben)
static int chR = -1, chG = -1, chB = -1;

// Warm-weiß Farbwerte
static const uint8_t WW_R = 255;
static const uint8_t WW_G = 180;
static const uint8_t WW_B =  60;

// ==================================================
// INITIALISIERUNG
// ==================================================
void lightInit()
{
    chR = ledcAttach(RGB_PIN_R, RGB_FREQ, RGB_BITS);
    chG = ledcAttach(RGB_PIN_G, RGB_FREQ, RGB_BITS);
    chB = ledcAttach(RGB_PIN_B, RGB_FREQ, RGB_BITS);

    if (chR < 0 || chG < 0 || chB < 0)
        Serial.println("❌ RGB LEDC: Kein Kanal verfügbar");
    else
        Serial.printf("✅ RGB LEDC R=%d G=%d B=%d\n", chR, chG, chB);

    // Sicher aus beim Start
    ledcWrite(chR, 0); ledcWrite(chG, 0); ledcWrite(chB, 0);
}

// ==================================================
// INTERNE HELFER
// ==================================================
static void rgbSet(uint8_t r, uint8_t g, uint8_t b)
{
    if (chR < 0) return;
    ledcWrite(chR, r);
    ledcWrite(chG, g);
    ledcWrite(chB, b);
}

static void rgbSetScaled(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness)
{
    // brightness 0–255 skaliert alle Kanäle
    rgbSet(
        (uint16_t)r * brightness / 255,
        (uint16_t)g * brightness / 255,
        (uint16_t)b * brightness / 255
    );
}

static void rgbOff()
{
    rgbSet(0, 0, 0);
}

// ==================================================
// LOCKLICHT
// ==================================================
void lightOn()
{
    digitalWrite(RELAIS_PIN, RELAY_ON);
    rgbSet(WW_R, WW_G, WW_B);
    Serial.println("💡 LICHT EIN");
}

void lightOff()
{
    digitalWrite(RELAIS_PIN, RELAY_OFF);
    rgbOff();
    dimmingActive = false;
    lightState    = LIGHT_OFF;
    Serial.println("💡 LICHT AUS");
}

// ==================================================
// STALLLICHT
// ==================================================
void stallLightOn()
{
    stallLightActive  = true;
    stallLightOffTime = millis() + (unsigned long)stallLightMinutes * 60000UL;
    digitalWrite(STALLLIGHT_RELAY_PIN, RELAY_ON);
    addLog("Stalllicht angeschaltet");
}

void stallLightOff()
{
    stallLightActive = false;
    digitalWrite(STALLLIGHT_RELAY_PIN, RELAY_OFF);
    addLog("Stalllicht ausgeschaltet");
}

// ==================================================
// RGB TESTMODUS ROT
// ==================================================
void rgbRedOn()
{
    rgbRedActive = true;
    rgbSet(255, 0, 0);
    Serial.println("🔴 RGB Rot EIN");
    addLog("🔴 Rotlicht EIN");
}

void rgbRedOff()
{
    rgbRedActive = false;
    rgbOff();
    Serial.println("⚫ RGB Rot AUS");
    addLog("🔴 Rotlicht AUS");
}

// ==================================================
// TIMER-HELFER
// ==================================================
void startLightForMinutes(int minutes)
{
    unsigned long addMs = (minutes <= 0) ? 10000UL : (unsigned long)minutes * 60000UL;
    unsigned long nowMs = millis();
    if (lightStateUntil > nowMs) lightStateUntil += addMs;
    else                         lightStateUntil  = nowMs + addMs;
}

void startLightForMinutesReset(int minutes)
{
    unsigned long addMs = (minutes <= 0) ? 10000UL : (unsigned long)minutes * 60000UL;
    lightStateUntil = millis() + addMs;
}

// ==================================================
// LICHT-ZUSTANDSMASCHINE
// ==================================================
void updateLightState()
{
    if (rgbRedActive)      return;
    if (manualLightActive) return;

    unsigned long nowMs = millis();

    switch (lightState)
    {
    case LIGHT_OFF:
        if (lightActive) { lightOff(); lightActive = false; }
        break;

    case LIGHT_PRE_OPEN:
    case LIGHT_POST_OPEN:
    case LIGHT_PRE_CLOSE:
    case LIGHT_POST_CLOSE:
        if (!lightActive) { lightOn(); lightActive = true; }
        if (nowMs >= lightStateUntil)
        {
            if (lightState == LIGHT_POST_OPEN)  addLog("Locklicht nach Öffnung beendet");
            if (lightState == LIGHT_POST_CLOSE) addLog("Locklicht nach Schließen beendet");
            lightState = LIGHT_OFF;
        }
        break;
    }
}

// ==================================================
// DIMMING UPDATE
// ==================================================
void updateDimming(unsigned long nowMs)
{
    if (!dimmingActive || !lightActive) return;
    if (nowMs >= dimStartTime && nowMs <= dimEndTime)
    {
        float    progress   = float(nowMs - dimStartTime) / float(dimEndTime - dimStartTime);
        progress            = constrain(progress, 0.0f, 1.0f);
        uint8_t  brightness = (uint8_t)constrain(int(255 * (1.0f - progress)), 0, 255);
        rgbSetScaled(WW_R, WW_G, WW_B, brightness);
    }
    if (nowMs > dimEndTime) dimmingActive = false;
}

// ==================================================
// STALLLICHT AUTO-AUS
// ==================================================
void updateStallLightTimer(unsigned long nowMs)
{
    if (stallLightActive && nowMs >= stallLightOffTime)
        stallLightOff();
}
