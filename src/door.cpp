#include "door.h"
#include "motor.h"
#include "light.h"
#include "logger.h"
#include "storage.h"
#include "system.h"
#include "pins.h"

// ==================================================
// GLOBALE VARIABLEN (dieses Moduls)
// ==================================================
bool         doorOpen         = false;
DoorPhase    doorPhase        = PHASE_IDLE;
unsigned long doorOpenedAt    = 0;

bool         learningActive   = false;
bool         learningOpenDone = false;
unsigned long learnStartTime  = 0;
bool         openedByLight    = false;
unsigned long manualOverrideUntil = 0;

bool          preLightOpenDone  = false;
bool          preLightCloseDone = false;
unsigned long lightAboveSince   = 0;
unsigned long lightBelowSince   = 0;
unsigned long plannedCloseAt    = 0;

// ==================================================
// TASTER KLAPPE
// ==================================================
void updateButton()
{
    static int lastRaw    = HIGH;
    static int stable     = HIGH;
    static unsigned long lastChange = 0;
    const unsigned long DEBOUNCE_MS = 30;

    extern unsigned long bootTime;
    int raw = digitalRead(BUTTON_PIN);
    if (raw != lastRaw) { lastChange = millis(); lastRaw = raw; }
    if ((millis() - lastChange) > DEBOUNCE_MS && raw != stable)
    {
        stable = raw;
        if (stable == LOW && millis() - bootTime > 500)
            handleButtonPress();
    }
}

// ==================================================
// TASTER STALLLICHT
// ==================================================
void updateStallButton()
{
    static int lastRaw    = HIGH;
    static int stable     = HIGH;
    static unsigned long lastChange = 0;
    const unsigned long DEBOUNCE_MS = 30;

    extern unsigned long bootTime;
    int raw = digitalRead(STALL_BUTTON_PIN);
    if (raw != lastRaw) { lastChange = millis(); lastRaw = raw; }
    if ((millis() - lastChange) > DEBOUNCE_MS && raw != stable)
    {
        stable = raw;
        if (stable == LOW && millis() - bootTime > 500)
        {
            if (stallLightActive) stallLightOff();
            else stallLightOn();
        }
    }
}

// ==================================================
// TASTER-AKTION
// ==================================================
void handleButtonPress()
{
    // ===== EINLERNEN =====
    if (learningActive)
    {
        if (!learningOpenDone)
        {
            openPosition = millis() - learnStartTime;
            saveMotorPositions();
            learningOpenDone = true;
            addLog("Open-Position gespeichert: " + String(openPosition));
            learnStartTime = millis();
            motorClose();
        }
        else
        {
            closePosition = millis() - learnStartTime;
            saveMotorPositions();
            learningActive = false;
            motorStop();
            addLog("Close-Position gespeichert: " + String(closePosition));
            addLog("Einlernen abgeschlossen");
        }
        return;
    }

    // ===== OTA/SAFESTATE =====
    if (otaInProgress || ioSafeState)
    {
        addLog("Taster ignoriert (OTA/Safe-State)");
        return;
    }

    // ===== MOTOR STOPP =====
    if (motorState != MOTOR_STOPPED)
    {
        motorStop();
        motorState  = MOTOR_STOPPED;
        motorReason = "Stop/Taster";
        doorPhase   = doorOpen ? PHASE_OPEN : PHASE_IDLE;
        addLog("Motor per Taster gestoppt");
        return;
    }

    // ===== TOGGLE =====
    if (doorOpen)
    {
        doorPhase   = PHASE_CLOSING;
        motorReason = "manuell/Taster";
        startMotorClose(closePosition);
        actionLock          = true;
        preLightOpenDone    = false;
        manualOverrideUntil = millis() + 300000UL;
        addLog("Schließvorgang gestartet (Taster)");
    }
    else
    {
        doorPhase   = PHASE_OPENING;
        motorReason = "manuell/Taster";
        startMotorOpen(openPosition);
        actionLock          = true;
        preLightCloseDone   = false;
        preLightOpenDone    = false;
        manualOverrideUntil = millis() + 300000UL;
        addLog("Öffnung gestartet (Taster)");
    }
}

// ==================================================
// TASTER ROTLICHT
// ==================================================
void updateRedButton()
{
    static int lastRaw    = HIGH;
    static int stable     = HIGH;
    static unsigned long lastChange = 0;
    const unsigned long DEBOUNCE_MS = 30;

    extern unsigned long bootTime;
    int raw = digitalRead(RED_BUTTON_PIN);
    if (raw != lastRaw) { lastChange = millis(); lastRaw = raw; }
    if ((millis() - lastChange) > DEBOUNCE_MS && raw != stable)
    {
        stable = raw;
        if (stable == LOW && millis() - bootTime > 500)
        {
            if (rgbRedActive) rgbRedOff();
            else              rgbRedOn();
        }
    }
}
