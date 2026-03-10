#pragma once

// ===============================
// WIFI CONFIGURATION
// ===============================

#define WIFI_SSID     "your_wifi_name"
#define WIFI_PASSWORD "your_wifi_password"

// ===============================
// MQTT CONFIGURATION
// ===============================

#define MQTT_SERVER   "192.168.1.100"
#define MQTT_PORT     1883
#define MQTT_USER     "mqtt_user"
#define MQTT_PASSWORD "mqtt_password"

// ===============================
// DEVICE SETTINGS
// ===============================

#define DEVICE_NAME      "chicken-coop-controller"
#define MQTT_BASE_TOPIC  "chickencoop"

// ===============================
// OPTIONAL FEATURES
// ===============================

#define ENABLE_MQTT true
#define ENABLE_OTA  true
