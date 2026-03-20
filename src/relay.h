#pragma once
#include <Arduino.h>

// ==================================================
// Relais-ESP – ESP-NOW Sender
//
// Sendet Relais-Befehle an einen zweiten ESP32
// der ein Relais schaltet.
//
// Wird NUR bei Automatik-Events ausgelöst:
//   - Lichtautomatik öffnet  → Relais AN
//   - Zeitautomatik öffnet   → Relais AN
//   - Lichtautomatik schließt → Relais AUS
//   - Zeitautomatik schließt  → Relais AUS
//
// Manuelle Bedienung (Web / Taster) hat keinen Einfluss.
//
// EEPROM Addr 480: relayEnabled (bool)
// EEPROM Addr 481–486: relayMac (6 Bytes)
// ==================================================

// Paketstruktur – muss im Relais-ESP-Sketch identisch sein!
struct RelayPacket {
    uint32_t magic;     // 0xC10534
    uint8_t  command;   // 0 = AUS, 1 = AN, 2 = Heartbeat
};
#define RELAY_PACKET_MAGIC  0xC10534u
#define RELAY_CMD_ON        1
#define RELAY_CMD_OFF       0
#define RELAY_CMD_HEARTBEAT 2

// Heartbeat-Statistik (für Web-UI)
extern unsigned long relayLastHeartbeat;  // millis() des letzten Heartbeats

// Konfiguration (wird aus EEPROM geladen)
extern bool    relayEnabled;
extern uint8_t relayMac[6];

void relayInit();                 // in setup() nach bmeInit()
void relayReset();                // nach MAC-Änderung aufrufen – registriert Peer neu
void relaySendOn();               // Relais AN (nur Automatik-Öffnung)
void relaySendOff();              // Relais AUS (nur Automatik-Schließung)
void relaySync();                 // in loop() aufrufen – sendet alle 60s den erwarteten Zustand
bool relayMacValid();             // true wenn MAC eingetragen

// Erwarteter Zustand (von Automatik gesetzt, manuell ignoriert)
extern bool relayExpectedState;
extern bool relayExpectedValid;   // true = Automatik hat mindestens einmal geschaltet
