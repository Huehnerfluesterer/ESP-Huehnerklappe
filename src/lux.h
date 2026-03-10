#pragma once
#include <Arduino.h>

// ==================================================
// LUX-SENSOR (VEML7700)
// ==================================================

// Aktueller gefilterter Lux-Wert (EMA-gefiltert, global genutzt)
extern float lux;

// Sensor-Status
extern bool  hasVEML;
extern bool  vemlHardError;
extern bool  vemlSoftError;

// Lux-Trend und -Rate (für Prognose)
extern float luxTrend;
extern float luxRateFiltered;

// Sensor bereit (erster stabiler Wert empfangen)
extern bool  luxReady;

// Zeitpunkt des letzten gültigen Messwerts (für Fallback-Erkennung)
extern unsigned long vemlLastLux;

// Gibt true zurück, wenn Lichtautomatik aktuell nutzbar ist
extern bool lightAutomationAvailable;

// Für Lux-Trend-Berechnung
extern float lastLux;
extern unsigned long lastLuxTime;

// Testmodus (simulierter Sonnenuntergang)
extern bool  forecastTestMode;
extern float testLuxStart;
extern float testLuxEnd;
extern float testDurationMin;
extern unsigned long testStartMillis;

// Sensor initialisieren (gibt true zurück wenn gefunden)
bool luxInit();

// Rohwert lesen (NAN bei Fehler)
float getLux();

// Medianfilter (Puffergröße 5)
float medianLux(float newValue);

// Trend-Historie aktualisieren
void updateLuxTrend(float currentLux);

// Fehlerüberwachung + ggf. Reinit auslösen (einmal pro Loop-Iteration aufrufen)
void checkLuxHealth(unsigned long nowMs, float rawLux, bool luxValid);

// I2C-Bus-Recovery (9 Clock-Pulse + STOP)
void i2cBusRecover();

// VEML7700 neu initialisieren (mit Cooldown-Schutz)
void reinitVEML7700();

// Automatische Gain-Anpassung
void autoRangeVEML(float luxValue);
