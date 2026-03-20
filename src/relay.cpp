#include "relay.h"
#include "logger.h"
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>

bool         relayEnabled        = false;
uint8_t      relayMac[6]         = {0, 0, 0, 0, 0, 0};
unsigned long relayLastHeartbeat = 0;
bool         relayExpectedState  = false;
bool         relayExpectedValid  = false;

static bool          peerAdded    = false;
static bool          espNowReady  = false;
static unsigned long lastSyncMs   = 0;
#define RELAY_SYNC_INTERVAL_MS  60000UL

// --------------------------------------------------
// Interne Hilfsfunktionen
// --------------------------------------------------


static bool ensureEspNow()
{
    if (espNowReady) return true;

    esp_err_t err = esp_now_init();
    if (err == ESP_OK) {
        // Kein register_recv_cb hier – läuft über espnow_dispatch.cpp
        espNowReady = true;
        Serial.println("[Relay] ESP-NOW initialisiert");
    } else if (err == ESP_ERR_ESPNOW_NOT_INIT) {
        espNowReady = false;
        return false;
    } else {
        // Bereits durch bme.cpp initialisiert
        espNowReady = true;
    }
    return true;
}

static bool ensurePeer()
{
    if (peerAdded) return true;
    if (!ensureEspNow()) return false;

    esp_now_del_peer(relayMac);

    uint8_t ch = (uint8_t)WiFi.channel();
    if (ch == 0) ch = 1;

    // Kanal explizit setzen damit ESP-NOW auf dem WLAN-Kanal sendet
    esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, relayMac, 6);
    peer.channel = ch;
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("[Relay] Peer konnte nicht registriert werden");
        return false;
    }
    peerAdded = true;
    Serial.printf("[Relay] Peer registriert: %02X:%02X:%02X:%02X:%02X:%02X Kanal %d\n",
        relayMac[0], relayMac[1], relayMac[2],
        relayMac[3], relayMac[4], relayMac[5], ch);
    return true;
}

static void sendCommand(uint8_t cmd)
{
    if (!relayEnabled)   { Serial.println("[Relay] Deaktiviert – kein Befehl"); return; }
    if (!relayMacValid()) { Serial.println("[Relay] Keine MAC konfiguriert");     return; }
    if (!ensurePeer())    return;

    RelayPacket pkt { RELAY_PACKET_MAGIC, cmd };
    esp_err_t err = esp_now_send(relayMac, (uint8_t*)&pkt, sizeof(pkt));
    if (err == ESP_OK)
        addLog(cmd == RELAY_CMD_ON ? "📡 Relais AN gesendet" : "📡 Relais AUS gesendet");
    else
        Serial.printf("[Relay] Sendefehler: %d\n", err);
}

// --------------------------------------------------
// Öffentliche API
// --------------------------------------------------

void relayInit()
{
    if (!relayEnabled || !relayMacValid()) return;
    ensurePeer();
}

void relayReset()
{
    // Peer entfernen falls vorhanden
    if (espNowReady) esp_now_del_peer(relayMac);
    peerAdded = false;
    // Neu registrieren
    relayInit();
}

bool relayMacValid()
{
    // MAC ist ungültig wenn alle Bytes 0 oder alle FF
    bool allZero = true, allFF = true;
    for (int i = 0; i < 6; i++) {
        if (relayMac[i] != 0x00) allZero = false;
        if (relayMac[i] != 0xFF) allFF   = false;
    }
    return !allZero && !allFF;
}

void relaySendOn()
{
    relayExpectedState = true;
    relayExpectedValid = true;
    sendCommand(RELAY_CMD_ON);
}

void relaySendOff()
{
    relayExpectedState = false;
    relayExpectedValid = true;
    sendCommand(RELAY_CMD_OFF);
}

void relaySync()
{
    if (!relayEnabled || !relayExpectedValid) return;
    unsigned long now = millis();
    if (now - lastSyncMs < RELAY_SYNC_INTERVAL_MS) return;
    lastSyncMs = now;

    // Nur senden wenn Relais-ESP online (Heartbeat innerhalb 90s)
    if (relayLastHeartbeat == 0 || now - relayLastHeartbeat > 90000UL) return;

    sendCommand(relayExpectedState ? RELAY_CMD_ON : RELAY_CMD_OFF);
    addLog(String("🔁 Relais-Sync: ") + (relayExpectedState ? "AN" : "AUS"));
}
