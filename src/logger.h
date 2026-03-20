#pragma once
#include <Arduino.h>

#define LOG_SIZE       100   // Max. Einträge im RAM-Puffer
#define LOG_FILE       "/logbook.txt"
#define LOG_MAX_LINES  100   // Max. Zeilen in der Datei

// Logbuch-Puffer (in logger.cpp definiert)
extern String logbook[LOG_SIZE];
extern int    logIndex;

// LittleFS initialisieren (einmalig in setup())
void loggerInit();

// Eintrag mit Zeitstempel (RTC oder Uptime-Fallback)
void addLog(const String &text);

// Eintrag mit Lux-Anhang
void addLogWithLux(const String &text, float lx);

// Alle Einträge löschen (RAM + LittleFS)
void clearLogbook();

// Auf Serial ausgeben
void serialLog(const String &msg);

// HTML-Darstellung aller Einträge (für Webseite)
String buildLogHTML();

// Logs aus LittleFS in RAM laden (beim Start)
void loggerLoadFromFS();
