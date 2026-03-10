#include "mqtt.h"
#include "storage.h"
#include "door.h"
#include "motor.h"
#include "light.h"
#include "lux.h"
#include "system.h"
#include "logger.h"
#include <WiFi.h>
#include <Arduino.h>
#include <ArduinoJson.h>


extern const char *FW_VERSION;

// ==================================================
// GLOBALE VARIABLEN
// ==================================================
WiFiClient   mqttWifi;
PubSubClient mqttClient(mqttWifi);

static unsigned long mqttLastConnectAttempt = 5000UL;  // 5s Boot-Verzögerung
static unsigned long mqttLastStatus         = 0;
const  unsigned long MQTT_STATUS_INTERVAL_MS = 10000UL;

// ==================================================
// HELPER
// ==================================================
static inline String t(const char *sub) { return String(mqttSettings.base) + "/" + sub; }
static inline void   mqttPublish(const String &topic, const String &payload, bool retained = false)
{
    mqttClient.publish(topic.c_str(), payload.c_str(), retained);
}

bool mqttClientConnected() { return mqttClient.connected(); }

// ==================================================
// PUBLISH
// ==================================================
void mqttPublishAvailability(const char *state)
{
    mqttPublish(t("tele/availability"), state, true);
}

void mqttPublishSettings(bool retained)
{
    JsonDocument doc;
    doc["openMode"]             = openMode;
    doc["closeMode"]            = closeMode;
    doc["openTime"]             = openTime;
    doc["closeTime"]            = closeTime;
    doc["openLightThreshold"]   = openLightThreshold;
    doc["closeLightThreshold"]  = closeLightThreshold;
    doc["lampPreOpen"]          = lampPreOpen;
    doc["lampPostOpen"]         = lampPostOpen;
    doc["lampPreClose"]         = lampPreClose;
    doc["lampPostClose"]        = lampPostClose;
    String out; serializeJson(doc, out);
    mqttPublish(t("tele/settings"), out, retained);
}

void mqttPublishStatus()
{
    DateTime nowDT = rtcOk ? rtc.now() : DateTime(2000, 1, 1, 0, 0, 0);
    JsonDocument doc;
    doc["time"]       = rtcOk ? nowDT.timestamp(DateTime::TIMESTAMP_TIME) : "--:--:--";
    doc["date"]       = rtcOk ? (String(nowDT.day()) + "." + String(nowDT.month()) + "." + String(nowDT.year())) : "-";
    doc["door"]       = doorOpen ? "Offen" : "Geschlossen";
    doc["moving"]     = (motorState != MOTOR_STOPPED) ? "1" : "0";
    doc["light"]      = isfinite(lux) ? String(lux, 1) : "n/a";
    doc["sensor"]     = (hasVEML && !vemlHardError) ? "VEML7700 OK" : "Lichtsensor Fehler";
    doc["lightState"] = lightActive ? "An" : "Aus";
    doc["stallLight"] = stallLightActive ? "An" : "Aus";
    doc["fw"]         = FW_VERSION;
    String out; serializeJson(doc, out);
    mqttPublish(t("tele/status"), out);
}

void mqttPublishLog(const String &line)
{
    mqttPublish(t("tele/log"), line);
}

// ==================================================
// SETTINGS APPLY
// ==================================================
bool applySettingsFromJson(const JsonDocument &doc, String &err)
{
    Settings s = settings;
    if (!doc["openMode"].isNull())          { strncpy(s.openMode,  doc["openMode"],  5);  s.openMode[5]  = '\0'; }
    if (!doc["closeMode"].isNull())         { strncpy(s.closeMode, doc["closeMode"], 5);  s.closeMode[5] = '\0'; }
    if (!doc["openTime"].isNull())          { strncpy(s.openTime,  doc["openTime"],  5);  s.openTime[5]  = '\0'; }
    if (!doc["closeTime"].isNull())         { strncpy(s.closeTime, doc["closeTime"], 5);  s.closeTime[5] = '\0'; }
    if (!doc["openLightThreshold"].isNull())  s.openLightThreshold  = doc["openLightThreshold"].as<int>();
    if (!doc["closeLightThreshold"].isNull()) s.closeLightThreshold = doc["closeLightThreshold"].as<int>();
    if (!doc["lampPreOpen"].isNull())   s.lampPreOpen   = doc["lampPreOpen"].as<int>();
    if (!doc["lampPostOpen"].isNull())  s.lampPostOpen  = doc["lampPostOpen"].as<int>();
    if (!doc["lampPreClose"].isNull())  s.lampPreClose  = doc["lampPreClose"].as<int>();
    if (!doc["lampPostClose"].isNull()) s.lampPostClose = doc["lampPostClose"].as<int>();

    if (s.openLightThreshold <= s.closeLightThreshold)
    {
        err = "Fehler: Öffnen-Lux muss größer sein als Schließen-Lux!";
        return false;
    }
    settings = s;
    saveSettings();
    applySettingsToRam();
    return true;
}

// ==================================================
// CALLBACK
// ==================================================
void mqttCallback(char *topic, byte *payload, unsigned int length)
{
    String top = String(topic);
    String pay; pay.reserve(length + 1);
    for (unsigned int i = 0; i < length; i++) pay += (char)payload[i];
    String cmd = pay; cmd.trim(); cmd.toUpperCase();

    // DOOR
    if (top == t("cmnd/door"))
    {
        if (otaInProgress || ioSafeState) { addLog("MQTT: Motor gesperrt"); return; }
        if (cmd == "OPEN" && !doorOpen && motorState == MOTOR_STOPPED)
        {
            doorPhase = PHASE_OPENING; motorReason = "manuell/MQTT";
            startMotorOpen(openPosition); actionLock = true;
            preLightCloseDone = false; preLightOpenDone = false;
            manualOverrideUntil = millis() + 300000UL;
            addLog("Öffnung gestartet (MQTT)");
        }
        else if (cmd == "CLOSE" && doorOpen && motorState == MOTOR_STOPPED)
        {
            doorPhase = PHASE_CLOSING; motorReason = "manuell/MQTT";
            startMotorClose(closePosition); actionLock = true;
            preLightOpenDone = false;
            manualOverrideUntil = millis() + 300000UL;
            addLog("Schließvorgang gestartet (MQTT)");
        }
        else if (cmd == "STOP" && motorState != MOTOR_STOPPED)
        {
            motorStop(); motorState = MOTOR_STOPPED;
            motorReason = "Stop/MQTT";
            doorPhase   = doorOpen ? PHASE_OPEN : PHASE_IDLE;
            addLog("Motor gestoppt (MQTT)");
        }
        else if (cmd == "TOGGLE")
        {
            if (motorState != MOTOR_STOPPED)
            {
                motorStop(); motorState = MOTOR_STOPPED;
                doorPhase = doorOpen ? PHASE_OPEN : PHASE_IDLE;
                addLog("Motor gestoppt (MQTT/Toggle)");
            }
            else if (doorOpen)
            {
                doorPhase = PHASE_CLOSING; motorReason = "manuell/MQTT";
                startMotorClose(closePosition); actionLock = true;
                manualOverrideUntil = millis() + 300000UL;
                addLog("Schließvorgang gestartet (MQTT/Toggle)");
            }
            else
            {
                doorPhase = PHASE_OPENING; motorReason = "manuell/MQTT";
                startMotorOpen(openPosition); actionLock = true;
                preLightCloseDone = false; preLightOpenDone = false;
                manualOverrideUntil = millis() + 300000UL;
                addLog("Öffnung gestartet (MQTT/Toggle)");
            }
        }
        mqttPublishStatus(); return;
    }

    // LIGHT
    if (top == t("cmnd/light"))
    {
        if (cmd == "ON")     { manualLightActive = true;  lightOn();  lightActive = true;  addLog("Locklicht AN (MQTT)"); }
        if (cmd == "OFF")    { manualLightActive = false; lightOff(); lightActive = false; addLog("Locklicht AUS (MQTT)"); }
        if (cmd == "TOGGLE")
        {
            if (manualLightActive) { manualLightActive = false; lightOff(); lightActive = false; addLog("Locklicht AUS (MQTT/Toggle)"); }
            else                   { manualLightActive = true;  lightOn();  lightActive = true;  addLog("Locklicht AN (MQTT/Toggle)"); }
        }
        mqttPublishStatus(); return;
    }

    // STALLLIGHT
    if (top == t("cmnd/stalllight"))
    {
        if (cmd == "ON") stallLightOn();
        else if (cmd == "OFF") stallLightOff();
        else if (cmd == "TOGGLE") { if (stallLightActive) stallLightOff(); else stallLightOn(); }
        mqttPublishStatus(); return;
    }

    // SETTINGS
    if (top == t("cmnd/settings"))
    {
        JsonDocument d;
        if (deserializeJson(d, pay)) { addLog("MQTT Settings JSON Fehler"); return; }
        String err;
        if (applySettingsFromJson(d, err)) { addLog("MQTT Settings übernommen"); mqttPublishSettings(true); mqttPublishStatus(); }
        else addLog("MQTT Settings FEHLER: " + err);
        return;
    }

    // GET
    if (top == t("cmnd/get"))
    {
        if (cmd == "STATUS")    mqttPublishStatus();
        else if (cmd == "SETTINGS") mqttPublishSettings(true);
        else if (cmd == "LOG")
        {
            for (int i = 0; i < LOG_SIZE; i++)
            {
                int idx = (logIndex + i) % LOG_SIZE;
                if (logbook[idx].length() > 0) mqttPublishLog(logbook[idx]);
            }
        }
        return;
    }

    // REBOOT
    if (top == t("cmnd/reboot") && cmd == "NOW")
    {
        addLog("MQTT: Reboot…");
        mqttPublishAvailability("offline");
        delay(100);
        ESP.restart();
    }
}

// ==================================================
// SETUP + LOOP
// ==================================================
void mqttSubscribeAll() { mqttClient.subscribe(t("cmnd/#").c_str()); }

void mqttSetup()
{
    mqttClient.setServer(mqttSettings.host, mqttSettings.port);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(1024);
    mqttClient.setKeepAlive(15);
    mqttClient.setSocketTimeout(3);
}

static void mqttEnsureConnected()
{
    // Nicht verbinden wenn deaktiviert oder kein gültiger Host
    if (!mqttSettings.enabled) return;
    if (strlen(mqttSettings.host) < 4) return;   // mind. "a.bc" – schützt vor EEPROM-Garbage
    if (mqttClient.connected()) return;

    unsigned long nowMs = millis();
    // Retry-Intervall 30s – connect() blockiert bis zu socketTimeout (5s)
    // und würde den Webserver bei 3s-Intervall dauerhaft einfrieren
    if (nowMs - mqttLastConnectAttempt < 30000UL) return;
    mqttLastConnectAttempt = nowMs;

    String willTopic = t("tele/availability");
    bool ok = (strlen(mqttSettings.user) > 0)
        ? mqttClient.connect(mqttSettings.clientId, mqttSettings.user, mqttSettings.pass, willTopic.c_str(), 0, true, "offline")
        : mqttClient.connect(mqttSettings.clientId, willTopic.c_str(), 0, true, "offline");

    if (ok)
    {
        addLog("MQTT verbunden");
        mqttPublishAvailability("online");
        mqttSubscribeAll();
        mqttPublishSettings(true);
        mqttPublishStatus();
    }
    else addLog("MQTT Verbindung fehlgeschlagen");
}

void mqttLoop()
{
    if (!mqttSettings.enabled)              return;
    if (strlen(mqttSettings.host) < 4)      return;
    if (!WiFi.isConnected())                return;
    mqttEnsureConnected();
    if (!mqttClient.connected()) return;
    mqttClient.loop();
    unsigned long nowMs = millis();
    if (nowMs - mqttLastStatus >= MQTT_STATUS_INTERVAL_MS)
    {
        mqttLastStatus = nowMs;
        mqttPublishStatus();
    }
}
