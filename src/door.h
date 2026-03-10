#pragma once
#include "types.h"
#include <Arduino.h>

extern bool         doorOpen;
extern DoorPhase    doorPhase;
extern unsigned long doorOpenedAt;

extern bool         learningActive;
extern bool         learningOpenDone;
extern unsigned long learnStartTime;
extern bool         openedByLight;
extern unsigned long manualOverrideUntil;

extern bool         preLightOpenDone;
extern bool         preLightCloseDone;
extern unsigned long lightAboveSince;
extern unsigned long lightBelowSince;
extern unsigned long plannedCloseAt;

// Taster-Logik (zyklisch aufrufen)
void updateButton();
void updateStallButton();
void updateRedButton();    // Taster Rotlicht (Toggle RGB-Rot)
void handleButtonPress();
