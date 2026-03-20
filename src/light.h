#pragma once
#include "types.h"
#include <Arduino.h>

// ==================================================
// LICHT-MODUL  –  12V RGB-Streifen via MOSFET/LEDC
// Warm-weiß: R=255 G=180 B=60  (über 3× IRLZ44N)
// ==================================================

// Konfigurierbare Lichtfarbe (über /rgb Seite)
extern uint8_t rgbColorR;
extern uint8_t rgbColorG;
extern uint8_t rgbColorB;
extern uint8_t rgbColorW;      // Warm-Weiß Kanal (RGBW)
extern uint8_t rgbBrightness;

extern LightState    lightState;
extern unsigned long lightStateUntil;
extern bool          lightActive;
extern bool          manualLightActive;
extern bool          stallLightActive;
extern unsigned long stallLightOffTime;
extern bool          rgbRedActive;       // war: ws2812RedActive

extern bool          dimmingActive;
extern unsigned long dimStartTime;
extern unsigned long dimEndTime;

// Laufzeit-Minuten für Stalllicht-Timer (Standard: 1)
extern int stallLightMinutes;

// Initialisierung (LEDC-Kanäle binden)
void lightInit();

// Locklicht (RGB warm-weiß)
void lightOn();
void lightOff();

// Stalllicht (zweites Relais, Timer-gesteuert)
void stallLightOn();
void stallLightOff();

// RGB Testmodus Rot (war: ws2812RedOn/Off)
void rgbRedOn();
void rgbRedOff();

// Licht-Zustandsmaschine (einmal pro Loop aufrufen)
void updateLightState();

// Timer-Helfer
void startLightForMinutes(int minutes);
void startLightForMinutesReset(int minutes);

// Dimming-Update (einmal pro Loop vor updateLightState)
void updateDimming(unsigned long nowMs);

// Stalllicht Auto-Aus (einmal pro Loop)
void updateStallLightTimer(unsigned long nowMs);
