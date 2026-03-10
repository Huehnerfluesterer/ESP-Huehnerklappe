#pragma once
#include <Arduino.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "types.h"

extern WiFiClient   mqttWifi;
extern PubSubClient mqttClient;

// Initialisierung (nach loadMqttSettings() aufrufen)
void mqttSetup();

// Zyklischer Update (in loop() aufrufen)
void mqttLoop();

// Verbindungsstatus
bool mqttClientConnected();

// Publish-Helfer
void mqttPublishAvailability(const char *state);
void mqttPublishStatus();
void mqttPublishSettings(bool retained = true);
void mqttPublishLog(const String &line);

// Settings aus JSON übernehmen (auch von Web-Seite genutzt)
bool applySettingsFromJson(const JsonDocument &doc, String &err);
