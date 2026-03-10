#include "storage.h"
#include "door.h"    // doorOpen, doorPhase
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

    if (strlen(mqttSettings.host) == 0 || strcmp(mqttSettings.host, "0.0.0.0") == 0)
    {
        mqttSettings.enabled = false;
        strcpy(mqttSettings.host,     "");       // leer → mqttLoop greift nie
        mqttSettings.port = 1883;
        strcpy(mqttSettings.user,     "");
        strcpy(mqttSettings.pass,     "");
        strcpy(mqttSettings.clientId, "");       // leer bis Benutzer konfiguriert
        strcpy(mqttSettings.base,     "Chickendoor");
        saveMqttSettings();
    }
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
