#pragma once
#include "types.h"

// EEPROM-Adressen
#define EEPROM_SIZE             512
#define EEPROM_ADDR_SETTINGS      0    // Settings-Struct
#define EEPROM_ADDR_OPEN_POS    100    // long openPosition
#define EEPROM_ADDR_CLOSE_POS   104    // long closePosition
#define EEPROM_ADDR_LIMIT_SW    120    // bool useLimitSwitches
#define EEPROM_ADDR_MQTT        200    // MqttSettings-Struct
#define EEPROM_ADDR_DOORSTATE   400    // bool doorOpen
#define EEPROM_ADDR_THEME       450    // char[10] theme
#define EEPROM_ADDR_BLOCKADE    460    // bool blockadeEnabled (1) + float blockadeThresholdA (4)

// Zugriff auf Settings (in storage.cpp definiert)
extern Settings     settings;
extern MqttSettings mqttSettings;

// Betriebseinstellungen (werden aus EEPROM geladen und im RAM gespiegelt)
extern String openMode;
extern String closeMode;
extern String openTime;
extern String closeTime;
extern int    openLightThreshold;
extern int    closeLightThreshold;
extern int    lampPreOpen;
extern int    lampPostOpen;
extern int    lampPreClose;
extern int    lampPostClose;

extern String uiTheme;

// Motorpositionen
extern long openPosition;
extern long closePosition;

// Endschalter-Flag
extern bool useLimitSwitches;

// EEPROM initialisieren (muss VOR allen load*()-Aufrufen geschehen)
void storageInit();

// Settings (Öffnen/Schließen/Licht/Lampen)
void saveSettings();
void loadSettings();
void applySettingsToRam();   // Struct → globale Variablen

// MQTT-Einstellungen
void saveMqttSettings();
void loadMqttSettings();

// Türzustand
void saveDoorState();
void loadDoorState();

// Theme
void saveTheme(const String &theme);
void loadTheme();

void saveBlockadeSettings();
void loadBlockadeSettings();

// Motor-Kalibrierung
void saveMotorPositions();
void loadMotorPositions();

// Endschalter-Einstellung
void loadLimitSwitchSetting();
