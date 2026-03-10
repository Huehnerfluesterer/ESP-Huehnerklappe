#pragma once
#include "types.h"
#include <Arduino.h>

// ==================================================
// MOTORSTEUERUNG
// ==================================================

extern MotorState    motorState;
extern unsigned long motorUntil;
extern String        motorReason;
extern int           MOTOR_CH;   // LEDC-Kanal (-1 = nicht zugewiesen)

// Motorpositionen (Laufzeit in ms, aus EEPROM geladen)
extern long openPosition;
extern long closePosition;

// Endschalter
extern bool         useLimitSwitches;
extern unsigned long limitOpenSince;
extern unsigned long limitCloseSince;

// Sperre: verhindert Automatik-Auslösung während manueller Aktion
extern bool actionLock;

// Initialisierung (LEDC-Kanal zuweisen, GPIO konfigurieren)
void motorInit();

// Hardware-Primitives
void motorStop();
void motorOpen();
void motorClose();

// Gesteuerte Starts mit Timeout
void startMotorOpen(unsigned long durationMs);
void startMotorClose(unsigned long durationMs);

// Rückwärtsfahren nach Blockade (kurz öffnen)
void reverseAfterBlockade();

// Gibt true zurück wenn Aktion manuell ausgelöst wurde (Taster/Web/MQTT)
bool isManualAction();

// Zyklischer Update: Endschalter + Timeout überwachen
void updateMotor();
