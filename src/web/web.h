#pragma once
#include <Arduino.h>
#include <WebServer.h>

extern WebServer server;

// Hilfsfunktionen (gemeinsam von allen Web-Dateien genutzt)
String renderThemeHead(const String &title);
String renderFooter();
int    timeToMinutes(const String &t);

// Startseite
void handleRoot();
void handleStatus();

// Einstellungen
void handleSettings();
void handleSaveOpen();
void handleSaveClose();

// Erweitert
void handleAdvanced();
void handleFw();
void handleSelftest();

// MQTT
void handleMqtt();
void handleSaveMqtt();

// Kalibrierung / Einlernen
void handleCalibration();
void handleLearn();
void handleLearnPage();

// Logbuch
void handleLogbook();
