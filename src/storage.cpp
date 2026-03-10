#include "storage.h"
#include "door.h"    // doorOpen, doorPhase
#include "motor.h"   // blockadeEnabled, blockadeThresholdA
#include "types.h"
#include <EEPROM.h>
#include <Arduino.h>

// ==================================================
// GLOBALE VARIABLEN (dieses Moduls)
// ==================================================
Settings     settings;
MqttSettings mqttSettings;

String openMode  = "time";
String closeMode = "time";
String openTime  = "07:00";
String closeTime = "20:00";

int openLightThreshold  = 300;
int closeLightThreshold = 200;

int lampPreOpen   = 5;
int lampPostOpen  = 5;
int lampPreClose  = 5;
int lampPostClose = 5;

String uiTheme = "auto";

long openPosition  = 6000;
long closePosition = 6000;

bool useLimitSwitches = true;

// ==================================================
void storageInit()
{
    EEPROM.begin(EEPROM_SIZE);
}

// ==================================================
// SETTINGS
// ==================================================
void applySettingsToRam()
{
    openMode  = settings.openMode;
    closeMode = settings.closeMode;
    openTime  = settings.openTime;
    closeTime = settings.closeTime;

    openLightThreshold  = settings.openLightThreshold;
    closeLightThreshold = settings.closeLightThreshold;

    lampPreOpen   = settings.lampPreOpen;
    lampPostOpen  = settings.lampPostOpen;
    lampPreClose  = settings.lampPreClose;
    lampPostClose = settings.lampPostClose;
}

void saveSettings()
{
    EEPROM.put(EEPROM_ADDR_SETTINGS, settings);
    EEPROM.commit();
}

void loadSettings()
{
    memset(&settings, 0, sizeof(settings));
    EEPROM.get(EEPROM_ADDR_SETTINGS, settings);

    if (strlen(settings.openMode) == 0)
    {
        // Standardwerte
        strcpy(settings.openMode,  "time");
        strcpy(settings.closeMode, "time");
        strcpy(settings.openTime,  "07:00");
        strcpy(settings.closeTime, "20:00");
        settings.openLightThreshold  = 300;
        settings.closeLightThreshold = 200;
        settings.lampPreOpen   = 5;
        settings.lampPostOpen  = 5;
        settings.lampPreClose  = 5;
        settings.lampPostClose = 5;
        saveSettings();
    }

    applySettingsToRam();
}

// ==================================================
// MQTT
// ==================================================
void saveMqttSettings()
{
    EEPROM.put(EEPROM_ADDR_MQTT, mqttSettings);
    EEPROM.commit();
}

void loadMqttSettings()
{
    EEPROM.get(EEPROM_ADDR_MQTT, mqttSettings);

    // Prüfen ob host druckbare ASCII-Zeichen enthält (kein Garbage)
    bool hostValid = true;
    for (int i = 0; i < (int)sizeof(mqttSettings.host); i++) {
        char c = mqttSettings.host[i];
        if (c == '\0') break;                        // Stringende → OK
        if (c < 32 || c > 126) { hostValid = false; break; }  // Kein druckbares ASCII → Garbage
    }

    if (!hostValid || strcmp(mqttSettings.host, "0.0.0.0") == 0)
    {
        memset(&mqttSettings, 0, sizeof(mqttSettings));
        mqttSettings.enabled = false;
        mqttSettings.port    = 1883;
        // alle Strings bleiben leer (\0 durch memset)
        saveMqttSettings();
    }

    // Port-Sanity (65535 = uninitialisiert)
    if (mqttSettings.port == 0 || mqttSettings.port == 65535)
        mqttSettings.port = 1883;
}

// ==================================================
// TÜRZUSTAND
// ==================================================
void saveDoorState()
{
    EEPROM.put(EEPROM_ADDR_DOORSTATE, doorOpen);
    EEPROM.commit();
}

void loadDoorState()
{
    EEPROM.get(EEPROM_ADDR_DOORSTATE, doorOpen);
    if (doorOpen != true && doorOpen != false)
    {
        doorOpen  = false;
        doorPhase = PHASE_IDLE;
        saveDoorState();
    }
}

// ==================================================
// THEME
// ==================================================
void saveTheme(const String &theme)
{
    uiTheme = theme;
    char buf[10];
    theme.toCharArray(buf, sizeof(buf));
    EEPROM.put(EEPROM_ADDR_THEME, buf);
    EEPROM.commit();
}

void loadTheme()
{
    char buf[10] = {0};
    EEPROM.get(EEPROM_ADDR_THEME, buf);
    uiTheme = (strlen(buf) == 0) ? "auto" : String(buf);
}

// ==================================================
// MOTORPOSITIONEN
// ==================================================
void saveMotorPositions()
{
    EEPROM.put(EEPROM_ADDR_OPEN_POS,  openPosition);
    EEPROM.put(EEPROM_ADDR_CLOSE_POS, closePosition);
    EEPROM.commit();
}

void loadMotorPositions()
{
    EEPROM.get(EEPROM_ADDR_OPEN_POS,  openPosition);
    EEPROM.get(EEPROM_ADDR_CLOSE_POS, closePosition);

    if (openPosition  < 1000 || openPosition  > 20000) openPosition  = 6000;
    if (closePosition < 1000 || closePosition > 20000) closePosition = 6000;
}

void loadLimitSwitchSetting()
{
    EEPROM.get(EEPROM_ADDR_LIMIT_SW, useLimitSwitches);
    if (useLimitSwitches != true && useLimitSwitches != false)
        useLimitSwitches = true;
}

void saveBlockadeSettings()
{
    EEPROM.put(EEPROM_ADDR_BLOCKADE,     blockadeEnabled);
    EEPROM.put(EEPROM_ADDR_BLOCKADE + 1, blockadeThresholdA);
    EEPROM.commit();
}

void loadBlockadeSettings()
{
    EEPROM.get(EEPROM_ADDR_BLOCKADE,     blockadeEnabled);
    EEPROM.get(EEPROM_ADDR_BLOCKADE + 1, blockadeThresholdA);
    // Sanitize
    if (blockadeEnabled != true && blockadeEnabled != false)
        blockadeEnabled = true;
    if (isnan(blockadeThresholdA) || blockadeThresholdA < 0.5f || blockadeThresholdA > 10.0f)
        blockadeThresholdA = 2.0f;
}
