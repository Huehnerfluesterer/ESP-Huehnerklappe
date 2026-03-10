#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <RTClib.h>

// ==================================================
// RTC
// ==================================================
extern RTC_DS3231 rtc;
extern bool       rtcOk;

// ==================================================
// OTA / Safe-State
// ==================================================
extern volatile bool otaInProgress;
extern volatile bool ioSafeState;

// ==================================================
// System-Health-Flags
// ==================================================
extern bool errorWifi;
extern bool errorMQTT;
extern bool errorSensor;   // true wenn VEML7700 ausgefallen

// Gibt true zurück wenn mind. ein kritischer Fehler vorliegt
bool systemError();

// Aktualisiert alle Health-Flags (WiFi, MQTT, Sensor)
void updateSystemHealth();

// Safe-State für OTA aktivieren / deaktivieren
void enterIoSafeState();
void leaveIoSafeState();
