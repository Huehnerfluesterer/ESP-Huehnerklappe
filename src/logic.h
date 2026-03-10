#pragma once
#include <Arduino.h>
#include <RTClib.h>

// ==================================================
// AUTOMATIK-LOGIK (Licht + Zeit + Prognose)
// ==================================================

// Fenster-Konstanten (Stunden)
#define OPEN_WINDOW_START_H   4
#define OPEN_WINDOW_END_H    14
#define CLOSE_WINDOW_START_H 15
#define CLOSE_WINDOW_END_H   23

// Zeiten und Raten
#define LIGHT_DELAY_MS          30000UL    // 30s stabile Dunkelheit für Schließen
#define LIGHT_OPEN_DELAY_MS     30000UL    // 30s stabile Helligkeit für Öffnen
#define LUX_TREND_INTERVAL_MS   30000UL    // Trend alle 30s neu
#define PRELIGHT_MIN_STABLE_MS  90000UL    // 90s Prognose-Stabilisierung Öffnen
#define CLOSE_FORECAST_STABLE_MS 180000UL  // 3 min Prognose-Stabilisierung Schließen
#define PRELIGHT_MAX_HOLD_MS    (45UL*60*1000)    // 45 min max Vor-Licht Öffnen
#define PRECLOSE_MAX_HOLD_MS    (120UL*60*1000)   // 2h max Vor-Licht Schließen
#define CLOSE_FORECAST_MAX_DISTANCE_LX 300

#define MIN_POS_LUX_RATE        0.15f
#define MIN_NEG_LUX_RATE        0.25f
#define LUX_RATE_ALPHA          0.2f
#define OPEN_FORECAST_TOLERANCE_MIN 10
#define OPEN_HYSTERESIS_LX      10
#define CLOSE_HYSTERESIS_LX     10
#define OPEN_GLITCH_MS          3000UL
// FIX: 3s war zu empfindlich – ein einzelner Glitch-Messwert (Auto, Taschenlampe)
// reichte um den 30s-Schließ-Timer zurückzusetzen. 30s entspricht LIGHT_DELAY_MS.
#define CLOSE_GLITCH_MS         30000UL
#define PRECLOSE_ABORT_POS_RATE    0.25f
#define PRECLOSE_ABORT_MARGIN_LX   40
#define PRECLOSE_ABORT_STABLE_MS   180000UL

// Prognose-Zustand (auch von motor.cpp referenziert für Reset)
extern bool          preLightForecastActive;
extern bool          preLightForecastCondition;
extern unsigned long preLightStartedAt;
extern bool          closeForecastCondition;
extern unsigned long closeForecastStableSince;
extern unsigned long scheduledCloseAt;
extern unsigned long openInterruptionSince;
extern unsigned long closeInterruptionSince;
extern bool          nightLock;
extern unsigned long lastDoorCloseTime;

// Hilfsfunktion: "HH:MM" → Minuten seit Mitternacht
int timeToMinutes(const String &t);

// Haupt-Automatik: einmal pro Loop aufrufen (nach Lux-Update)
void runAutomatik(const DateTime &now, int nowMin, unsigned long nowMs,
                  bool luxValid, bool luxReady, float luxRateFiltered);
