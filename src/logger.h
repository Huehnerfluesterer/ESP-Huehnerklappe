#pragma once
#include <Arduino.h>

#define LOG_SIZE 120

// Logbuch-Puffer (in logger.cpp definiert)
extern String logbook[LOG_SIZE];
extern int    logIndex;

// Eintrag mit Zeitstempel (RTC oder Uptime-Fallback)
void addLog(const String &text);

// Eintrag mit Lux-Anhang: "… – 70.3 lx" bzw. "… – n/a lx"
void addLogWithLux(const String &text, float lx);

// Alle Einträge löschen
void clearLogbook();

// Auf Serial ausgeben (Wrapper, damit kein direktes Serial.println im Code verteilt ist)
void serialLog(const String &msg);

// HTML-Darstellung aller Einträge (für Webseite)
String buildLogHTML();
