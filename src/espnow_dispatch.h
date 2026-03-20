#pragma once
#include <Arduino.h>

// ==================================================
// ESP-NOW Dispatcher
//
// Nur EIN esp_now_register_recv_cb erlaubt.
// Diese Datei ist der einzige Empfänger – leitet
// Pakete anhand des Magic-Bytes weiter an bme.cpp
// und relay.cpp.
// ==================================================

void espnowDispatcherInit();   // einmalig in setup() nach bmeInit()/relayInit()
