// ==========================
// FIRMWARE VERSION
// ==========================
const char *FW_VERSION = "1.0.15";

// ==========================
// INCLUDES
// ==========================
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <WebServer.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include <PubSubClient.h>

#include "src/pins.h"
#include "src/types.h"
#include "src/storage.h"
#include "src/system.h"
#include "src/logger.h"
#include "src/lux.h"
#include "src/light.h"
#include "src/motor.h"
#include "src/door.h"
#include "src/logic.h"
#include "src/mqtt.h"
#include "src/wlan.h"
#include "src/web/web.h"
#include "src/icons.h"       // icon192_png / icon512_png PROGMEM-Arrays

// ==========================
// GLOBALE OBJEKTE
// ==========================
WebServer server(80);
unsigned long bootTime    = 0;
unsigned long lastLogicRun = 0;
const unsigned long LOGIC_INTERVAL = 200;

// Lux-Zustand für Loop
static unsigned long lastLuxRead   = 0;
static unsigned long lastTrendUpdate = 0;
static float         luxFiltered   = 0.0f;
static bool          luxInitDone   = false;

// ACS712 (Strom-Kalibrierung)
float currentBaseline   = 0.0f;
float currentThreshold  = 0.0f;
bool  currentCalibrated = false;

int stallLightMinutes = 1;   // in light.cpp referenziert

// ==========================
// SETUP
// ==========================
void setup()
{
    // WDT
    esp_task_wdt_config_t wdt_cfg = { .timeout_ms = 20000, .idle_core_mask = 3, .trigger_panic = true };
    esp_task_wdt_init(&wdt_cfg);
    esp_task_wdt_add(NULL);

    Serial.begin(115200);
    Serial.println("\n🐔 Hühnerklappe – FW " + String(FW_VERSION));

    // ===== EEPROM (muss zuerst) =====
    storageInit();

    // ===== EINSTELLUNGEN LADEN =====
    loadMqttSettings();
    loadSettings();
    loadDoorState();
    loadMotorPositions();
    loadTheme();
    loadLimitSwitchSetting();

    // ===== GPIO =====
    pinMode(MOTOR_IN1,          OUTPUT); digitalWrite(MOTOR_IN1, LOW);
    pinMode(MOTOR_IN2,          OUTPUT); digitalWrite(MOTOR_IN2, LOW);
    pinMode(RELAIS_PIN,         OUTPUT); digitalWrite(RELAIS_PIN, RELAY_OFF);
    pinMode(STALLLIGHT_RELAY_PIN,OUTPUT);digitalWrite(STALLLIGHT_RELAY_PIN, RELAY_OFF);
    pinMode(BUTTON_PIN,         INPUT_PULLUP);
    pinMode(STALL_BUTTON_PIN,   INPUT_PULLUP);
    pinMode(LIMIT_OPEN_PIN,     INPUT_PULLUP);
    pinMode(LIMIT_CLOSE_PIN,    INPUT_PULLUP);
    pinMode(WS2812_PIN,         OUTPUT); digitalWrite(WS2812_PIN, LOW);

    // ===== WS2812 + MOTOR =====
    stallLight.begin();
    stallLight.clear();
    stallLight.show();
    motorInit();

    // ===== I2C + RTC + VEML =====
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(30000);
    Wire.setTimeOut(50);
    rtcOk = rtc.begin();
    if (!rtcOk) Serial.println("⚠️ RTC DS3231 nicht gefunden");
    luxInit();

    // ===== WIFI + NTP =====
    WiFi.mode(WIFI_STA);
    WiFi.setHostname("Huehnerklappe-ESP32");
    wifiConnectNonBlocking();

    configTzTime("CET-1CEST,M3.5.0/2,M10.5.0/3", "pool.ntp.org", "time.nist.gov");
    {
        struct tm ti;
        static bool rtcSynced = false;
        if (!rtcSynced && getLocalTime(&ti, 2000))
        {
            if (rtcOk) rtc.adjust(DateTime(ti.tm_year+1900, ti.tm_mon+1, ti.tm_mday, ti.tm_hour, ti.tm_min, ti.tm_sec));
            Serial.println("✅ RTC per NTP synchronisiert");
            rtcSynced = true;
        }
    }

    // ===== TÜRZUSTAND =====
    if (doorOpen) { doorPhase = PHASE_OPEN; doorOpenedAt = millis(); }
    else          { doorPhase = PHASE_IDLE; }
    preLightOpenDone = false;
    lightAboveSince  = 0;
    lightBelowSince  = 0;

    // ===== MQTT =====
    mqttSetup();

    // ===== WEBSERVER ROUTEN =====
    server.on("/",              handleRoot);
    server.on("/status",        handleStatus);
    server.on("/settings",      handleSettings);
    server.on("/save-open",  HTTP_POST, handleSaveOpen);
    server.on("/save-close", HTTP_POST, handleSaveClose);
    server.on("/advanced",   HTTP_GET,  handleAdvanced);
    server.on("/fw",         HTTP_GET,  handleFw);
    server.on("/systemtest", HTTP_GET,  handleSelftest);
    server.on("/mqtt",       HTTP_GET,  handleMqtt);
    server.on("/save-mqtt",  HTTP_POST, handleSaveMqtt);
    server.on("/calibration",   handleCalibration);
    server.on("/learn",         handleLearn);
    server.on("/learn-page",    handleLearnPage);
    server.on("/learn-start", HTTP_POST, handleLearn);
    server.on("/log",           handleLogbook);

    server.on("/mini",   []() { server.send(200, "text/plain", "OK"); });

    server.on("/log/clear", HTTP_POST, []() {
        if (otaInProgress || ioSafeState) { server.send(503, "text/plain", "OTA aktiv"); return; }
        clearLogbook(); addLog("Logbuch manuell gelöscht");
        server.send(200, "text/plain", "OK");
    });

    server.on("/set-theme", HTTP_POST, []() {
        String t = server.arg("theme");
        if (t == "dark" || t == "light" || t == "auto") saveTheme(t);
        server.send(200, "text/plain", "OK");
    });

    server.on("/set-limit-switches", HTTP_POST, []() {
        if (server.hasArg("enabled")) {
            useLimitSwitches = server.arg("enabled") == "1";
            EEPROM.put(EEPROM_ADDR_LIMIT_SW, useLimitSwitches); EEPROM.commit();
            addLog(String("Endschalter ") + (useLimitSwitches ? "aktiviert" : "deaktiviert"));
        }
        server.send(200, "text/plain", "OK");
    });

    server.on("/door", []() {
        if (otaInProgress || ioSafeState) { server.send(503, "text/plain", "OTA aktiv"); return; }
        if (motorState != MOTOR_STOPPED) {
            motorStop(); motorState = MOTOR_STOPPED; motorReason = "Stop/Manuell";
            doorPhase = doorOpen ? PHASE_OPEN : PHASE_IDLE;
            addLog("Motor gestoppt"); server.send(200, "text/plain", "STOP"); return;
        }
        if (doorOpen) {
            doorPhase = PHASE_CLOSING; motorReason = "manuell/Web (Toggle)";
            startMotorClose(closePosition); actionLock = true;
            preLightOpenDone = false; manualOverrideUntil = millis() + 300000UL;
            server.send(200, "text/plain", "Closing"); addLog("Schließvorgang gestartet (Toggle)");
        } else {
            doorPhase = PHASE_OPENING; motorReason = "manuell/Web (Toggle)";
            startMotorOpen(openPosition); actionLock = true;
            preLightCloseDone = false; preLightOpenDone = false; manualOverrideUntil = millis() + 300000UL;
            server.send(200, "text/plain", "Opening"); addLog("Öffnung gestartet (Toggle)");
        }
    });

    server.on("/open", []() {
        if (otaInProgress || ioSafeState) { server.send(503, "text/plain", "OTA aktiv"); return; }
        if (doorOpen) { server.send(200, "text/plain", "Already open"); return; }
        if (motorState != MOTOR_STOPPED) { server.send(200, "text/plain", "Motor running"); return; }
        doorPhase = PHASE_OPENING; motorReason = "manuell/Web";
        startMotorOpen(openPosition); actionLock = true;
        preLightCloseDone = false; preLightOpenDone = false; manualOverrideUntil = millis() + 300000UL;
        server.send(200, "text/plain", "Opening");
    });

    server.on("/close", []() {
        if (otaInProgress || ioSafeState) { server.send(503, "text/plain", "OTA aktiv"); return; }
        if (!doorOpen) { server.send(200, "text/plain", "Already closed"); return; }
        if (motorState != MOTOR_STOPPED) { server.send(200, "text/plain", "Motor running"); return; }
        doorPhase = PHASE_CLOSING; motorReason = "manuell/Web";
        startMotorClose(closePosition); actionLock = true;
        preLightOpenDone = false; manualOverrideUntil = millis() + 300000UL;
        server.send(200, "text/plain", "Closing");
    });

    server.on("/light", []() {
        if (manualLightActive) { manualLightActive = false; lightOff(); lightActive = false; addLog("Locklicht manuell AUS"); server.send(200, "text/plain", "OFF"); }
        else { manualLightActive = true; lightOn(); lightActive = true; addLog("Locklicht manuell AN"); server.send(200, "text/plain", "ON"); }
    });

    server.on("/stalllight", []() {
        if (stallLightActive) { stallLightOff(); server.send(200, "text/plain", "OFF"); }
        else { stallLightOn(); server.send(200, "text/plain", "ON"); }
    });

    server.on("/ws2812red", []() {
        if (ws2812RedActive) { ws2812RedOff(); server.send(200, "text/plain", "OFF"); }
        else { ws2812RedOn(); server.send(200, "text/plain", "ON"); }
    });

    server.on("/motor/up", []() {
        if (otaInProgress || ioSafeState) { server.send(503, "text/plain", "Motor gesperrt"); return; }
        if (motorState == MOTOR_STOPPED) { motorReason = "Service"; startMotorOpen(openPosition); }
        server.send(200, "text/plain", "OK");
    });

    server.on("/motor/down", []() {
        if (otaInProgress || ioSafeState) { server.send(503, "text/plain", "Motor gesperrt"); return; }
        if (motorState == MOTOR_STOPPED) { motorReason = "Service"; startMotorClose(closePosition); }
        server.send(200, "text/plain", "OK");
    });

    server.on("/motor/stop", []() { motorStop(); motorState = MOTOR_STOPPED; server.send(200, "text/plain", "OK"); });

    server.on("/calib-status", []() {
        server.send(200, "application/json", "{\"open\":" + String(openPosition) + ",\"close\":" + String(closePosition) + "}");
    });

    server.on("/learn-status", []() {
        server.send(200, "application/json", "{\"active\":" + String(learningActive?"true":"false") + ",\"phase\":" + String(learningOpenDone?2:1) + "}");
    });

    server.on("/systemtest-status", HTTP_GET, []() {
        updateSystemHealth();
        StaticJsonDocument<320> doc;
        doc["wifi"]           = (WiFi.status() == WL_CONNECTED);
        doc["rssi"]           = WiFi.RSSI();
        doc["mqtt"]           = mqttClientConnected();
        doc["lux"]            = (hasVEML && !vemlHardError) ? lux : -1;
        doc["bhOk"]           = (hasVEML && !vemlHardError);
        doc["rtcOk"]          = rtcOk ? 1 : 0;
        doc["rtcStatus"]      = rtcOk ? "OK" : "Nicht gefunden / nicht initialisiert";
        doc["heap"]           = ESP.getFreeHeap();
        doc["uptime"]         = millis() / 1000;
        doc["useLimitSwitches"] = useLimitSwitches;
        String out; serializeJson(doc, out);
        server.send(200, "application/json", out);
    });

    server.on("/systemtest-motor", HTTP_POST, []() {
        if (motorState == MOTOR_STOPPED && !otaInProgress && !ioSafeState) {
            startMotorOpen(200); delay(250); motorStop();
        }
        server.send(200, "text/plain", "OK");
    });

    server.on("/mqtt-test", HTTP_POST, []() {
        WiFiClient tc; PubSubClient tm(tc);
        tm.setServer(server.arg("host").c_str(), server.arg("port").toInt());
        String user = server.arg("user");
        bool ok = user.length() > 0
            ? tm.connect(server.arg("clientId").c_str(), user.c_str(), server.arg("pass").c_str())
            : tm.connect(server.arg("clientId").c_str());
        if (ok) { tm.disconnect(); server.send(200, "text/plain", "OK"); }
        else server.send(200, "text/plain", "FAIL");
    });

    server.on("/reset", HTTP_POST, []() {
        server.send(200, "text/plain", "Restarting");
        delay(500); ESP.restart();
    });

    server.on("/manifest.json", HTTP_GET, []() {
        server.send(200, "application/json", R"JSON({
  "name":"Hühnerklappe","short_name":"Klappe",
  "start_url":"/","scope":"/","display":"standalone",
  "background_color":"#4CAF50","theme_color":"#4CAF50",
  "icons":[{"src":"/icon-192.png","sizes":"192x192","type":"image/png"},
           {"src":"/icon-512.png","sizes":"512x512","type":"image/png"}]
})JSON");
    });

    server.on("/icon-192.png", HTTP_GET, []() {
        server.send_P(200, "image/png", (PGM_P)icon192_png, icon192_png_len);
    });
    server.on("/icon-512.png", HTTP_GET, []() {
        server.send_P(200, "image/png", (PGM_P)icon512_png, icon512_png_len);
    });

    // Forecast-Test
    server.on("/test/forecast/start", HTTP_GET, []() {
        forecastTestMode = true; luxReady = true; testStartMillis = millis();
        lastLux = testLuxStart; lastLuxTime = millis(); luxRateFiltered = 0;
        addLog("TEST: Prognose-Sonnenuntergang gestartet");
        server.send(200, "text/plain", "Prognose-Test gestartet");
    });
    server.on("/test/forecast/stop", HTTP_GET, []() {
        forecastTestMode = false; addLog("TEST: Prognose-Test beendet");
        server.send(200, "text/plain", "Prognose-Test beendet");
    });

    // OTA
    server.on("/update", HTTP_POST,
        []() {
            bool ok = Update.end(true);
            server.send(ok ? 200 : 500, "text/plain; charset=UTF-8", ok ? "Update erfolgreich" : "Update fehlgeschlagen");
            leaveIoSafeState(); otaInProgress = false;
            if (ok) { delay(300); ESP.restart(); }
        },
        []() {
            if (!otaInProgress) { otaInProgress = true; enterIoSafeState(); }
            HTTPUpload &u = server.upload();
            if      (u.status == UPLOAD_FILE_START)  { if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial); }
            else if (u.status == UPLOAD_FILE_WRITE)  { esp_task_wdt_reset(); if (Update.write(u.buf, u.currentSize) != u.currentSize) Update.printError(Serial); }
            else if (u.status == UPLOAD_FILE_ABORTED){ Update.abort(); leaveIoSafeState(); otaInProgress = false; }
        }
    );

    // ===== RELAIS-TEST (Boot-Indikator) =====
    lightOn();  delay(3000);
    lightOff(); delay(500);

    bootTime = millis();
    server.begin();
    addLog("🚀 Hühnerklappe gestartet – FW " + String(FW_VERSION));
}

// ==========================
// LOOP
// ==========================
void loop()
{
    const unsigned long nowMs = millis();

    if (nowMs - lastLogicRun < LOGIC_INTERVAL) { delay(1); return; }
    lastLogicRun = nowMs;

    // ===== WEBSERVER / WDT / NETZWERK =====
    server.handleClient();
    esp_task_wdt_reset();
    mqttLoop();
    wifiWatchdog();

    // ===== MOTOR + TASTER =====
    updateMotor();
    updateButton();
    updateStallButton();

    // ===== LUX LESEN (alle 1s) =====
    float rawLux = NAN;
    bool  luxValid = false;
    bool  luxReadAttempted = false;

    if (nowMs - lastLuxRead > 1000)
    {
        lastLuxRead      = nowMs;
        luxReadAttempted = true;

        if (forecastTestMode)
        {
            float elapsed = (nowMs - testStartMillis) / 60000.0f;
            if (elapsed > testDurationMin) elapsed = testDurationMin;
            rawLux = testLuxStart - (testLuxStart - testLuxEnd) * (elapsed / testDurationMin);
        }
        else rawLux = getLux();

        if (isfinite(rawLux)) rawLux = medianLux(rawLux);
    }
    luxValid = isfinite(rawLux) && rawLux >= 0.0f;

    // ===== LUX-FEHLERÜBERWACHUNG =====
    if (luxReadAttempted)
        checkLuxHealth(nowMs, rawLux, luxValid);

    // ===== EMA FILTER =====
    if (luxValid)
    {
        vemlLastLux = nowMs;
        if (!luxInitDone) { luxFiltered = rawLux; luxInitDone = true; }
        else luxFiltered = luxFiltered * 0.8f + rawLux * 0.2f;
        lux = luxFiltered;
    }

    // ===== TREND =====
    if (!luxReady && lux > 5)
    {
        luxReady   = true;
        lastLux    = lux;
        lastLuxTime = nowMs;
        luxRateFiltered = 0;
    }
    if (luxValid && nowMs - lastTrendUpdate > 30000)
    {
        updateLuxTrend(lux);
        lastTrendUpdate = nowMs;
    }

    // ===== SYSTEM-HEALTH =====
    updateSystemHealth();

    // ===== DIMMING + STALLLICHT =====
    updateDimming(nowMs);
    updateStallLightTimer(nowMs);

    // ===== OHNE RTC: nur Licht-Zustand halten =====
    if (!rtcOk) { updateLightState(); return; }

    // ===== AUTOMATIK =====
    DateTime now   = rtc.now();
    int      nowMin = now.hour() * 60 + now.minute();
    runAutomatik(now, nowMin, nowMs, luxValid, luxReady, luxRateFiltered);

    // ===== LICHT-ZUSTANDSMASCHINE =====
    updateLightState();
}
