#include "logic.h"
#include "light.h"
#include "motor.h"
#include "door.h"
#include "lux.h"
#include "storage.h"
#include "logger.h"
#include "relay.h"
#include <math.h>

// ==================================================
// GLOBALE VARIABLEN (dieses Moduls)
// ==================================================
bool          preLightForecastActive    = false;
bool          preLightForecastCondition = false;
unsigned long preLightStartedAt         = 0;

bool          closeForecastCondition    = false;
unsigned long closeForecastStableSince  = 0;

unsigned long scheduledCloseAt          = 0;
unsigned long openInterruptionSince     = 0;
unsigned long closeInterruptionSince    = 0;

bool          nightLock                 = false;
unsigned long lastDoorCloseTime         = 0;

// Lokale Hilfsvariablen
static bool          preLightHoldEnabled   = true;
static bool          preCloseHoldEnabled   = true;
static unsigned long preCloseStartedAt     = 0;
static unsigned long closeBrightTrendSince = 0;
static int           lastOpenActionMin      = -1;
static int           lastCloseActionMin     = -1;
static int           nightLockResetDay      = -1;
static unsigned long preLightForecastSince  = 0;

// Aus lux.h
extern float luxRateFiltered;
extern float lastLux;
extern unsigned long lastLuxTime;

// ==================================================
int timeToMinutes(const String &t)
{
    int sep = t.indexOf(':');
    if (sep == -1) return 0;
    return t.substring(0, sep).toInt() * 60 + t.substring(sep + 1).toInt();
}

// ==================================================
void runAutomatik(const DateTime &now, int nowMin, unsigned long nowMs,
                  bool luxValid, bool luxReady, float /*luxRateFiltered_unused*/)
{
    const bool automatikErlaubt = (nowMs >= manualOverrideUntil);

    // ===== SAFETY: actionLock zurücksetzen wenn Motor schon steht =====
    // Verhindert dauerhaften Automatik-Block wenn z.B. ein Web-/MQTT-Toggle
    // eintraf während die Tür bereits in der gewünschten Position war und der
    // Motor deshalb nie gestartet wurde.
    if (motorState == MOTOR_STOPPED)
        actionLock = false;

    // ===== NACHT-SPERRE RESET (morgens, einmalig) =====
    if (openMode == "light" && closeMode == "light" && nightLock &&
        now.hour() >= OPEN_WINDOW_START_H && now.hour() < CLOSE_WINDOW_START_H)
    {
        if (now.day() != nightLockResetDay)
        {
            nightLock        = false;
            nightLockResetDay = now.day();
            addLog("Nacht-Sperre zurückgesetzt");
        }
    }

    const bool inOpenWindow  = (now.hour() >= OPEN_WINDOW_START_H  && now.hour() <= OPEN_WINDOW_END_H);
    const bool inCloseWindow = (now.hour() >= CLOSE_WINDOW_START_H && now.hour() <= CLOSE_WINDOW_END_H);

    // ===== LUX-TREND BERECHNEN =====
    if (luxValid && lastLuxTime > 0 && nowMs - lastLuxTime >= LUX_TREND_INTERVAL_MS)
    {
        float dtMin = (nowMs - lastLuxTime) / 60000.0f;
        if (dtMin > 0.0f)
        {
            float luxRate    = (lux - lastLux) / dtMin;
            luxRateFiltered  = LUX_RATE_ALPHA * luxRate + (1.0f - LUX_RATE_ALPHA) * luxRateFiltered;
            lastLux          = lux;
            lastLuxTime      = nowMs;
        }
    }

    // ===== ZEITMODUS: VOR-LICHT ÖFFNEN =====
    if (automatikErlaubt && openMode == "time" && doorPhase == PHASE_IDLE && !doorOpen && !preLightOpenDone)
    {
        int openMin = timeToMinutes(openTime);
        int diffMin = openMin - nowMin;
        if (diffMin < 0) diffMin += 1440;
        if (diffMin > 0 && diffMin <= lampPreOpen)
        {
            lightState = LIGHT_PRE_OPEN;
            startLightForMinutesReset(lampPreOpen);
            addLog("Locklicht vor Öffnung (Zeitmodus)");
            preLightOpenDone = true;
        }
    }

    // ===== ZEITMODUS: VOR-LICHT SCHLIESSEN =====
    if (automatikErlaubt && closeMode == "time" && doorPhase == PHASE_OPEN && doorOpen && !preLightCloseDone)
    {
        int closeMin = timeToMinutes(closeTime);
        int diffMin  = closeMin - nowMin;
        if (diffMin < 0) diffMin += 1440;
        if (diffMin > 0 && diffMin <= lampPreClose)
        {
            lightState = LIGHT_PRE_CLOSE;
            startLightForMinutes(lampPreClose);
            addLog("Locklicht vor Schließen (Zeitmodus)");
            preLightCloseDone = true;
        }
    }

    // ===== LICHTMODUS: ÖFFNEN =====
    if (automatikErlaubt && luxValid && openMode == "light" && !doorOpen && !learningActive && !nightLock)
    {
        if (!inOpenWindow)
        {
            if (lightState == LIGHT_PRE_OPEN)
            {
                lightState = LIGHT_OFF;
                addLog("ℹ️ Öffnungsfenster zu – Vor-Licht beendet");
            }
            lightAboveSince    = 0;
            openInterruptionSince = 0;
        }
        else
        {
            // ---- 1) Vor-Licht per Prognose ----
            bool forecastNow = false;
            if (luxRateFiltered > MIN_POS_LUX_RATE && lux < openLightThreshold)
            {
                float minutesToThresh = (openLightThreshold - lux) / luxRateFiltered;
                if (minutesToThresh >= (float)lampPreOpen - OPEN_FORECAST_TOLERANCE_MIN &&
                    minutesToThresh <= (float)lampPreOpen + OPEN_FORECAST_TOLERANCE_MIN)
                    forecastNow = true;
            }

            if (forecastNow != preLightForecastCondition)
            {
                preLightForecastCondition = forecastNow;
                preLightForecastSince     = nowMs;
            }

            if ((nowMs - preLightForecastSince) >= PRELIGHT_MIN_STABLE_MS)
            {
                if (lux <= openLightThreshold * 0.6f)
                {
                    if (preLightForecastCondition && !preLightForecastActive)
                    {
                        lightState = LIGHT_PRE_OPEN;
                        startLightForMinutesReset(max(1, lampPreOpen));
                        addLogWithLux("Locklicht vor Öffnung gestartet (Prognose stabil)", lux);
                        preLightForecastActive = true;
                        preLightStartedAt      = nowMs;
                    }
                    if (!preLightForecastCondition && preLightForecastActive)
                    {
                        const bool safetyTimeout    = (nowMs - preLightStartedAt) >= PRELIGHT_MAX_HOLD_MS;
                        const bool thresholdNotReached = lux < openLightThreshold;
                        if (preLightHoldEnabled && thresholdNotReached && !safetyTimeout)
                        {
                            if (lightState == LIGHT_PRE_OPEN && (long)(lightStateUntil - nowMs) < 20000L)
                                startLightForMinutesReset(1);
                        }
                        else
                        {
                            lightState             = LIGHT_OFF;
                            addLog("Locklicht vor Öffnung beendet (Prognose verworfen/Timeout)");
                            preLightForecastActive = false;
                        }
                    }
                }
            }
        }

        // ---- 2) Öffnung durch Schwelle ----
        if (!actionLock && !doorOpen && lux >= openLightThreshold)
        {
            openInterruptionSince = 0;
            if (lightAboveSince == 0) lightAboveSince = nowMs;
            if (nowMs - lightAboveSince >= LIGHT_OPEN_DELAY_MS && motorState == MOTOR_STOPPED)
            {
                doorPhase   = PHASE_OPENING;
                motorReason = "Lichtautomatik";
                startMotorOpen(openPosition);
                actionLock       = true;
                preLightCloseDone = false;
                preLightOpenDone  = false;
                lightAboveSince   = 0;
                openInterruptionSince = 0;
                addLog("Öffnung gestartet (Schwellen-Erfüllung)");
                relaySendOn();
            }
        }
        else if (lux < openLightThreshold - OPEN_HYSTERESIS_LX)
        {
            if (openInterruptionSince == 0) openInterruptionSince = nowMs;
            if (nowMs - openInterruptionSince > OPEN_GLITCH_MS)
                { lightAboveSince = 0; openInterruptionSince = 0; }
        }
    }

    // ===== ZEITMODUS ÖFFNEN =====
    if (automatikErlaubt && !learningActive && !doorOpen && !nightLock)
    {
        if ((openMode == "time" || (openMode == "light" && !lightAutomationAvailable)) &&
            nowMin == timeToMinutes(openTime) && lastOpenActionMin != nowMin)
        {
            if (motorState == MOTOR_STOPPED)
            {
                doorPhase   = PHASE_OPENING;
                motorReason = (openMode == "light" && !lightAutomationAvailable) ? "Zeit-Fallback" : "Zeitautomatik";
                startMotorOpen(openPosition);
                actionLock          = true;
                lastOpenActionMin   = nowMin;
                preLightCloseDone   = false;
                preLightOpenDone    = false;
                relaySendOn();
            }
        }
    }

    // ===== ZEITMODUS SCHLIESSEN =====
    if (automatikErlaubt && !learningActive && doorOpen)
    {
        if ((closeMode == "time" || (closeMode == "light" && !lightAutomationAvailable)) &&
            nowMin == timeToMinutes(closeTime) && lastCloseActionMin != nowMin)
        {
            if (motorState == MOTOR_STOPPED)
            {
                doorPhase   = PHASE_CLOSING;
                motorReason = (closeMode == "light" && !lightAutomationAvailable) ? "Zeit-Fallback" : "Zeitautomatik";
                startMotorClose(closePosition);
                lastCloseActionMin = nowMin;
                addLog("Schließvorgang gestartet (" + motorReason + ")");
                preLightOpenDone = false;
                relaySendOff();
            }
        }
    }

    // ===== SCHLIESS-PROGNOSE =====
    bool forecastNow =
        doorOpen && !manualLightActive &&
        lux > closeLightThreshold &&
        lux < closeLightThreshold + CLOSE_FORECAST_MAX_DISTANCE_LX &&
        luxRateFiltered < -MIN_NEG_LUX_RATE;

    if (forecastNow != closeForecastCondition)
        { closeForecastCondition = forecastNow; closeForecastStableSince = nowMs; }

    if (closeForecastCondition && nowMs - closeForecastStableSince > CLOSE_FORECAST_STABLE_MS)
    {
        float minutesToThresh = (lux - closeLightThreshold) / (-luxRateFiltered);
        if (!isfinite(minutesToThresh) || minutesToThresh < 0 || minutesToThresh > 180)
            minutesToThresh = 9999;

        if (doorOpen && minutesToThresh <= lampPreClose && !preLightCloseDone)
        {
            lightState = LIGHT_PRE_CLOSE;
            startLightForMinutesReset(lampPreClose);
            addLogWithLux("Locklicht vor Schließen gestartet (stabile Prognose)", lux);
            preCloseStartedAt = nowMs;
            preLightCloseDone = true;
        }
    }

    if (scheduledCloseAt != 0 && nowMs >= scheduledCloseAt && !preLightCloseDone)
    {
        if (!manualLightActive)
        {
            lightState = LIGHT_PRE_CLOSE;
            startLightForMinutesReset(max(1, lampPreClose));
            addLogWithLux("Locklicht vor Schließen gestartet (Prognose/Termin)", lux);
            preCloseStartedAt      = nowMs;
            closeBrightTrendSince  = 0;
        }
        scheduledCloseAt = 0;
    }

    // ===== PRE-CLOSE HOLD =====
    if (preCloseHoldEnabled && lightState == LIGHT_PRE_CLOSE)
    {
        const bool safetyTimeout = (nowMs - preCloseStartedAt) >= PRECLOSE_MAX_HOLD_MS;
        if (!safetyTimeout && (long)(lightStateUntil - nowMs) < 20000L)
            startLightForMinutesReset(1);

        if (!inCloseWindow)
        {
            scheduledCloseAt = 0;
            lightState       = LIGHT_OFF;
            addLog("Locklicht vor Schließen beendet (Fenster)");
        }
        else
        {
            const bool abortCandidate =
                (luxRateFiltered > PRECLOSE_ABORT_POS_RATE) &&
                (lux >= closeLightThreshold + PRECLOSE_ABORT_MARGIN_LX);

            if (abortCandidate)
            {
                if (closeBrightTrendSince == 0) closeBrightTrendSince = nowMs;
                if (nowMs - closeBrightTrendSince >= PRECLOSE_ABORT_STABLE_MS)
                {
                    if (!preCloseHoldEnabled || safetyTimeout)
                    {
                        scheduledCloseAt = 0;
                        lightState       = LIGHT_OFF;
                        addLog("Locklicht vor Schließen abgebrochen (stabil deutlich heller)");
                    }
                }
            }
            else closeBrightTrendSince = 0;
        }
    }

    // ===== SCHLIESSEN BEI SCHWELLEN-ERFÜLLUNG =====
    // FIX: inCloseWindow absichtlich NICHT geprüft.
    // Der Lux-Sensor funktioniert → wenn es dunkel genug ist, schließen wir.
    // inCloseWindow würde frühe Sonnenuntergänge im Winter (vor 15:00) blockieren
    // und ist bei funktionierendem Sensor unnötig. Der Zeit-Fallback hat sein
    // eigenes Fenster über closeTime.
    const int closeRiseThreshold = closeLightThreshold + CLOSE_HYSTERESIS_LX;

    if (!actionLock && doorOpen && motorState == MOTOR_STOPPED &&
        lightAutomationAvailable && lux <= closeLightThreshold)
    {
        closeInterruptionSince = 0;
        if (lightBelowSince == 0) lightBelowSince = nowMs;
        if (nowMs - lightBelowSince >= LIGHT_DELAY_MS)
        {
            doorPhase   = PHASE_CLOSING;
            motorReason = "Lichtautomatik";
            startMotorClose(closePosition);
            actionLock          = true;
            lightBelowSince     = 0;
            closeInterruptionSince = 0;
            scheduledCloseAt    = 0;
            addLog("Schließvorgang gestartet (Schwellen-Erfüllung)");
            relaySendOff();
        }
    }
    else if (lux > closeRiseThreshold)
    {
        if (closeInterruptionSince == 0) closeInterruptionSince = nowMs;
        if (nowMs - closeInterruptionSince > CLOSE_GLITCH_MS)
            { lightBelowSince = 0; closeInterruptionSince = 0; }
    }

    // Sensor ausgefallen: Timer zurücksetzen
    if (!lightAutomationAvailable)
        { lightBelowSince = 0; closeInterruptionSince = 0; }
}


