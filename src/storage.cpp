#include "storage.h"
#include "door.h"    // doorOpen, doorPhase
#include "motor.h"   // blockadeEnabled, blockadeThresholdA
#include "bme.h"     // bmeSource
#include "relay.h"   // relayEnabled, relayMac
#include "light.h"   // rgbColorR/G/B, rgbBrightness
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

    // Magic-Byte prüfen – wenn fehlt → EEPROM frisch oder nach OTA korrupt
    uint8_t magic = EEPROM.read(EEPROM_MAGIC_ADDR);
    if (magic != EEPROM_MAGIC_VALUE) {
        Serial.println("⚠️ EEPROM Magic fehlt – behalte vorhandene Werte, setze nur fehlende Defaults");
        // Motorpositionen prüfen und ggf. Default setzen
        long op, cp;
        EEPROM.get(EEPROM_ADDR_OPEN_POS,  op);
        EEPROM.get(EEPROM_ADDR_CLOSE_POS, cp);
        if (op  < 1000 || op  > 20000) { op  = 6000; EEPROM.put(EEPROM_ADDR_OPEN_POS,  op); }
        if (cp  < 1000 || cp  > 20000) { cp  = 6000; EEPROM.put(EEPROM_ADDR_CLOSE_POS, cp); }
        // Magic setzen
        EEPROM.write(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
        EEPROM.commit();
        Serial.printf("✅ EEPROM initialisiert – openPos=%ld closePos=%ld\n", op, cp);
    }
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

    // Sanitize – Bereich 500ms bis 60s
    if (openPosition  < 500  || openPosition  > 60000) openPosition  = 6000;
    if (closePosition < 500  || closePosition > 60000) closePosition = 6000;
    Serial.printf("📦 EEPROM: openPos=%ld ms, closePos=%ld ms\n", openPosition, closePosition);
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

void saveBmeSource()
{
    uint8_t val = (uint8_t)bmeSource;
    EEPROM.put(EEPROM_ADDR_BME_SOURCE, val);
    EEPROM.commit();
}

void loadBmeSource()
{
    uint8_t val = 0;
    EEPROM.get(EEPROM_ADDR_BME_SOURCE, val);
    // Sanitize: nur 0 oder 1 gültig
    bmeSource = (val == 1) ? BME_SOURCE_ESPNOW : BME_SOURCE_LOCAL;
}

void saveRelaySettings()
{
    EEPROM.put(EEPROM_ADDR_RELAY,     relayEnabled);
    EEPROM.put(EEPROM_ADDR_RELAY + 1, relayMac);
    EEPROM.commit();
}

void loadRelaySettings()
{
    EEPROM.get(EEPROM_ADDR_RELAY,     relayEnabled);
    EEPROM.get(EEPROM_ADDR_RELAY + 1, relayMac);
    // Sanitize
    if (relayEnabled != true) relayEnabled = false;
}

void saveRgbSettings()
{
    EEPROM.put(EEPROM_ADDR_RGB,     rgbColorR);
    EEPROM.put(EEPROM_ADDR_RGB + 1, rgbColorG);
    EEPROM.put(EEPROM_ADDR_RGB + 2, rgbColorB);
    EEPROM.put(EEPROM_ADDR_RGB + 3, rgbBrightness);
    EEPROM.commit();
}

void loadRgbSettings()
{
    uint8_t r, g, b, br;
    EEPROM.get(EEPROM_ADDR_RGB,     r);
    EEPROM.get(EEPROM_ADDR_RGB + 1, g);
    EEPROM.get(EEPROM_ADDR_RGB + 2, b);
    EEPROM.get(EEPROM_ADDR_RGB + 3, br);
    // Sanitize – ungültige EEPROM-Werte → Warm-Weiß Default
    if (r == 0xFF && g == 0xFF && b == 0xFF) {
        rgbColorR = 255; rgbColorG = 197; rgbColorB = 143; rgbBrightness = 255;
    } else {
        rgbColorR = r; rgbColorG = g; rgbColorB = b;
        rgbBrightness = (br == 0) ? 255 : br;
    }
}
