#pragma once
#include <Arduino.h>

// ==================================================
// BME280 – Temperatur, Luftfeuchtigkeit, Luftdruck
//
// Zwei Betriebsmodi (Laufzeit umschaltbar, EEPROM):
//   BME_SOURCE_LOCAL  – Sensor direkt am I²C-Bus
//   BME_SOURCE_ESPNOW – Empfang per ESP-NOW von
//                       einem zweiten ESP32
// ==================================================

enum BmeSource
{
    BME_SOURCE_LOCAL  = 0,
    BME_SOURCE_ESPNOW = 1
};

// Daten (immer gültig, egal woher)
extern bool      bmeOk;          // true = Werte aktuell und plausibel
extern float     bmeTemp;        // °C
extern float     bmeHumidity;    // %
extern float     bmePressure;    // hPa
extern BmeSource bmeSource;      // aktiver Modus

// ESP-NOW-Statistik (nur im ESPNOW-Modus relevant)
extern unsigned long bmeLastReceived;   // millis() des letzten Pakets
extern uint8_t       bmeLastSenderMac[6];

// Initialisierung & Update
void bmeInit();                  // einmalig in setup(), nach Wire.begin()
void bmeUpdate();                // in loop() aufrufen (nicht-blockierend)

// Modus wechseln (speichert nicht ins EEPROM – das macht storage.cpp)
void bmeSetSource(BmeSource src);

// Paketstruktur – muss in Sender-Sketch identisch sein!
struct BmePacket
{
    uint32_t magic;      // 0xBEEF280 – Erkennungsmuster
    float    temp;
    float    humidity;
    float    pressure;
};
#define BME_PACKET_MAGIC  0xBEEF280u
