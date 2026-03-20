// ==================================================
// BME280 ESP-NOW Sender
// Hühnerklappe – Außensensor
//
// Sendet alle 10s Temperatur, Luftfeuchtigkeit und
// Luftdruck per ESP-NOW an die Hühnerklappe.
//
// SETUP:
//   1. WIFI_SSID und WIFI_PASSWORD eintragen
//   2. Sketch flashen – fertig!
//      Keine MAC-Adresse nötig.
//
// HARDWARE:
//   BME280 an I²C: SDA=GPIO21, SCL=GPIO22, VCC=3.3V
//   Adresse 0x76 (SDO→GND) oder 0x77 (SDO→VCC)
// ==================================================

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <Wire.h>
#include <Adafruit_BME280.h>

// --------------------------------------------------
// ⚙️ KONFIGURATION – hier anpassen!
// --------------------------------------------------

// WLAN-Zugangsdaten (nur für Kanal-Synchronisation nötig)
#define WIFI_SSID      ""
#define WIFI_PASSWORD  ""

// Sendeintervall
#define SEND_INTERVAL_MS  10000UL

// BME280 I²C-Adresse (0x76 oder 0x77)
#define BME_ADDRESS  0x76

// --------------------------------------------------
// Broadcast-MAC – kein Eintrag nötig!
// ESP-NOW sendet an alle Empfänger im Netz
// --------------------------------------------------
static uint8_t BROADCAST_MAC[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// --------------------------------------------------
// Paketstruktur – muss identisch mit bme.h sein!
// --------------------------------------------------
struct BmePacket {
    uint32_t magic;
    float    temp;
    float    humidity;
    float    pressure;
};
#define BME_PACKET_MAGIC  0xBEEF280u

// --------------------------------------------------
// Globale Objekte
// --------------------------------------------------
static Adafruit_BME280 bme;
static bool            bmeFound  = false;
static bool            peerAdded = false;
static unsigned long   lastSend  = 0;
static uint32_t        sendCount = 0;

// --------------------------------------------------
// ESP-NOW Sende-Callback
// --------------------------------------------------
static void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status)
{
    if (status == ESP_NOW_SEND_SUCCESS)
        Serial.printf("[ESP-NOW] Paket #%lu gesendet ✓\n", sendCount);
    else
        Serial.printf("[ESP-NOW] Paket #%lu FEHLGESCHLAGEN ✗\n", sendCount);
}

// --------------------------------------------------
// SETUP
// --------------------------------------------------
void setup()
{
    Serial.begin(115200);
    delay(3000);
    Serial.println("\n🌡 BME280 ESP-NOW Sender");
    Serial.println("========================");

    // I²C + BME280
    Wire.begin(21, 22);
    bmeFound = bme.begin(BME_ADDRESS);
    if (bmeFound) {
        bme.setSampling(
            Adafruit_BME280::MODE_NORMAL,
            Adafruit_BME280::SAMPLING_X2,
            Adafruit_BME280::SAMPLING_X16,
            Adafruit_BME280::SAMPLING_X1,
            Adafruit_BME280::FILTER_X16,
            Adafruit_BME280::STANDBY_MS_500
        );
        Serial.printf("✅ BME280 gefunden (0x%02X)\n", BME_ADDRESS);
    } else {
        Serial.println("⚠️ Kein BME280 – sende Testdaten");
    }

    // WiFi verbinden – wichtig damit ESP-NOW auf dem gleichen Kanal
    // wie die Hühnerklappe läuft
    Serial.printf("📡 Verbinde mit %s ...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
        delay(250); Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n✅ WLAN verbunden – Kanal %d\n", WiFi.channel());
    } else {
        Serial.println("\n⚠️ WLAN nicht verbunden – ESP-NOW könnte auf falschem Kanal sein!");
    }

    Serial.printf("📡 Eigene MAC: %s\n", WiFi.macAddress().c_str());
    Serial.println("   (Diese MAC wird in der Klappe automatisch erkannt)");

    // ESP-NOW init
    if (esp_now_init() != ESP_OK) {
        Serial.println("❌ ESP-NOW Init fehlgeschlagen – Neustart");
        delay(3000); ESP.restart();
    }
    esp_now_register_send_cb(onSent);

    // Broadcast-Peer registrieren
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BROADCAST_MAC, 6);
    peer.channel = 0;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) == ESP_OK) {
        peerAdded = true;
        Serial.println("✅ Broadcast-Peer registriert");
    } else {
        Serial.println("❌ Peer Fehler");
    }

    Serial.printf("Sende alle %lu s\n\n", SEND_INTERVAL_MS / 1000);
}

// --------------------------------------------------
// LOOP
// --------------------------------------------------
void loop()
{
    const unsigned long now = millis();
    if (now - lastSend < SEND_INTERVAL_MS) return;
    lastSend = now;

    if (!peerAdded) return;

    float t, h, p;
    if (!bmeFound) {
        t = 20.0f; h = 55.0f; p = 1013.0f;
        Serial.println("[TEST] Dummy-Werte");
    } else {
        t = bme.readTemperature();
        h = bme.readHumidity();
        p = bme.readPressure() / 100.0f;
        if (isnan(t) || t < -40 || t > 85 ||
            isnan(h) || h < 0   || h > 100 ||
            isnan(p) || p < 300 || p > 1100) {
            Serial.println("⚠️ Messung unplausibel");
            return;
        }
    }

    Serial.printf("[Messung] %.1f°C  %.1f%%  %.1f hPa\n", t, h, p);
    sendCount++;
    BmePacket pkt { BME_PACKET_MAGIC, t, h, p };
    esp_now_send(BROADCAST_MAC, (uint8_t*)&pkt, sizeof(pkt));
}
