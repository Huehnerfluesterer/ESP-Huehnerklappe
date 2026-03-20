#include "bme.h"
#include "logger.h"
#include <Adafruit_BME280.h>
#include <esp_now.h>
#include <WiFi.h>

// ==================================================
// GLOBALE VARIABLEN
// ==================================================
bool          bmeOk            = false;
float         bmeTemp          = NAN;
float         bmeHumidity      = NAN;
float         bmePressure      = NAN;
BmeSource     bmeSource        = BME_SOURCE_LOCAL;
unsigned long bmeLastReceived  = 0;
uint8_t       bmeLastSenderMac[6] = {0};

static Adafruit_BME280 bme;
static bool            bmeLocalFound = false;
static bool            espNowActive  = false;
static unsigned long   lastLocalRead = 0;

static const unsigned long ESPNOW_TIMEOUT_MS = 60000UL;

// ==================================================
// ESP-NOW RECEIVE CALLBACK
// ==================================================

// ==================================================
// INTERNE HELPERS
// ==================================================
static void startEspNow()
{
    if (espNowActive) return;
    if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init fehlgeschlagen");
        addLog("ESP-NOW Init fehlgeschlagen");
        return;
    }
    // Kein register_recv_cb hier – läuft über espnow_dispatch.cpp
    espNowActive = true;
    Serial.println("ESP-NOW gestartet (BME280 Empfang)");
    Serial.printf("Empfaenger-MAC: %s\n", WiFi.macAddress().c_str());
    addLog("ESP-NOW BME280 aktiv, MAC: " + WiFi.macAddress());
}

static void stopEspNow()
{
    if (!espNowActive) return;
    esp_now_unregister_recv_cb();
    esp_now_deinit();
    espNowActive = false;
    bmeOk = false;
}

static void readLocal()
{
    if (!bmeLocalFound) return;
    const unsigned long now = millis();
    if (now - lastLocalRead < 10000UL) return;
    lastLocalRead = now;

    float t = bme.readTemperature();
    float h = bme.readHumidity();
    float p = bme.readPressure() / 100.0f;

    if (isnan(t) || t < -40.0f || t > 85.0f)   return;
    if (isnan(h) || h <   0.0f || h > 100.0f)  return;
    if (isnan(p) || p < 300.0f || p > 1100.0f) return;

    bmeTemp = t; bmeHumidity = h; bmePressure = p; bmeOk = true;
}

// ==================================================
// INIT
// ==================================================
void bmeInit()
{
    bmeLocalFound = bme.begin(0x76) || bme.begin(0x77);
    if (bmeLocalFound) {
        bme.setSampling(
            Adafruit_BME280::MODE_NORMAL,
            Adafruit_BME280::SAMPLING_X2,
            Adafruit_BME280::SAMPLING_X16,
            Adafruit_BME280::SAMPLING_X1,
            Adafruit_BME280::FILTER_X16,
            Adafruit_BME280::STANDBY_MS_500
        );
        Serial.println("BME280 lokal gefunden");
    } else {
        Serial.println("Kein lokaler BME280 gefunden");
    }
    bmeSetSource(bmeSource);
}

// ==================================================
// MODUS WECHSELN
// ==================================================
void bmeSetSource(BmeSource src)
{
    bmeSource = src; bmeOk = false; bmeTemp = NAN; bmeHumidity = NAN; bmePressure = NAN;
    if (src == BME_SOURCE_ESPNOW) {
        stopEspNow();   // erst stoppen falls schon aktiv (z.B. nach WLAN-Reset)
        startEspNow();
    } else {
        stopEspNow();
        if (bmeLocalFound) lastLocalRead = 0;
    }
}

// ==================================================
// UPDATE (loop)
// ==================================================
void bmeUpdate()
{
    if (bmeSource == BME_SOURCE_LOCAL) {
        readLocal();
    } else {
        if (bmeOk && bmeLastReceived > 0 && millis() - bmeLastReceived > ESPNOW_TIMEOUT_MS) {
            bmeOk = false;
            addLog("BME280 ESP-NOW Timeout");
        }
    }
}
