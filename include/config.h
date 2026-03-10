#pragma once

// ⚠️  HINWEIS: Diese Datei enthält Zugangsdaten.
//     Niemals in ein öffentliches Git-Repository einchecken!
//     → config.h in .gitignore eintragen.

#define WIFI_SSID     "YOUR_SSID"
#define WIFI_PASSWORD "YOUR_PASSWORD"

#define MQTT_SERVER   "192.168.1.10"
#define MQTT_PORT     1883
#define MQTT_USER     "user"
#define MQTT_PASSWORD "password"

#define DEVICE_NAME     "huehnerklappe"
#define MQTT_BASE_TOPIC "chickencoop"

#define ENABLE_MQTT true
#define ENABLE_OTA  true
