#include "espnow_dispatch.h"
#include "bme.h"
#include "relay.h"
#include <esp_now.h>
#include <Arduino.h>

// Zentraler Empfangs-Callback – einziger registrierter recv_cb
static void onEspNowReceive(const uint8_t *mac, const uint8_t *data, int len)
{
    if (len < 4) return;  // Mindestens magic (uint32_t)

    uint32_t magic;
    memcpy(&magic, data, sizeof(magic));

    if (magic == BME_PACKET_MAGIC) {
        // BME280-Paket
        if (bmeSource != BME_SOURCE_ESPNOW) return;
        if (len != (int)sizeof(BmePacket)) return;

        BmePacket pkt;
        memcpy(&pkt, data, sizeof(pkt));

        if (isnan(pkt.temp)     || pkt.temp     < -40.0f || pkt.temp     > 85.0f)  return;
        if (isnan(pkt.humidity) || pkt.humidity <   0.0f || pkt.humidity > 100.0f) return;
        if (isnan(pkt.pressure) || pkt.pressure < 300.0f || pkt.pressure > 1100.0f)return;

        bmeTemp         = pkt.temp;
        bmeHumidity     = pkt.humidity;
        bmePressure     = pkt.pressure;
        bmeOk           = true;
        bmeLastReceived = millis();
        memcpy(bmeLastSenderMac, mac, 6);
    }
    else if (magic == RELAY_PACKET_MAGIC) {
        // Relais-Paket (Heartbeat vom Relais-ESP)
        if (len != (int)sizeof(RelayPacket)) return;

        RelayPacket pkt;
        memcpy(&pkt, data, sizeof(pkt));

        if (pkt.command == RELAY_CMD_HEARTBEAT) {
            relayLastHeartbeat = millis();
        }
    }
}

void espnowDispatcherInit()
{
    // Alten Callback überschreiben – ab jetzt läuft alles durch hier
    esp_now_register_recv_cb(onEspNowReceive);
}
