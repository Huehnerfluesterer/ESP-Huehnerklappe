#include "wlan.h"
#include "logger.h"
#include "bme.h"
#include <WiFi.h>
#include <Arduino.h>
#include "config.h"

static unsigned long lastWifiCheck    = 0;
static unsigned long wifiBackoffMs    = 0;
static unsigned long lastReconnectTry = 0;

// Boot-Gnadenfrist: Watchdog greift erst nach 30s ein.
// WiFi braucht manchmal 10–15s – ohne Frist würde der Hard-Reset
// die laufende Verbindung abreißen bevor sie zustande kommt.
static const unsigned long WIFI_BOOT_GRACE_MS = 30000UL;

void wifiConnectNonBlocking()
{
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    addLog("🔌 WLAN: Verbinde ...");
}

static void wifiHardResetStack()
{
    addLog("♻️ WLAN-Stack Neustart");
    // ESP-NOW zuerst beenden – sonst hängt esp_now_deinit nach WiFi.mode(OFF)
    if (bmeSource == BME_SOURCE_ESPNOW) bmeSetSource(BME_SOURCE_ESPNOW); // stoppt + merkt Modus
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    wifiConnectNonBlocking();
    // ESP-NOW nach WiFi-Neustart wieder aktivieren
    if (bmeSource == BME_SOURCE_ESPNOW) bmeSetSource(BME_SOURCE_ESPNOW);
}

void wifiWatchdog()
{
    unsigned long nowMs = millis();

    // Während der Boot-Gnadenfrist nur prüfen ob schon verbunden – nie eingreifen
    if (nowMs < WIFI_BOOT_GRACE_MS) {
        if (WiFi.status() == WL_CONNECTED) wifiBackoffMs = 0;
        return;
    }

    if (nowMs - lastWifiCheck < 5000) return;
    lastWifiCheck = nowMs;

    if (WiFi.status() == WL_CONNECTED) { wifiBackoffMs = 0; return; }

    if (wifiBackoffMs == 0) wifiBackoffMs = 5000;
    else wifiBackoffMs = min(wifiBackoffMs * 2, (unsigned long)60000);

    if (nowMs - lastReconnectTry >= wifiBackoffMs)
    {
        lastReconnectTry = nowMs;
        addLog("📡 WLAN weg – versuche Reconnect");
        WiFi.disconnect(false, false);
        WiFi.reconnect();

        static uint8_t hardTries = 0;
        if (++hardTries >= 3) { hardTries = 0; wifiHardResetStack(); }
    }
}
