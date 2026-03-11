// ==========================
// FIRMWARE VERSION
// ==========================
const char *FW_VERSION = "2.0.16";

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
// esp_task_wdt nicht manuell verwenden – Arduino-Framework übernimmt das
#include <PubSubClient.h>

#include "pins.h"
#include "types.h"
#include "storage.h"
#include "system.h"
#include "logger.h"
#include "lux.h"
#include "light.h"
#include "motor.h"
#include "door.h"
#include "logic.h"
#include "mqtt.h"
#include "wlan.h"
#include "web/web.h"
#include "icons.h"       // icon192_png / icon512_png PROGMEM-Arrays

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

// ACS712 – Variablen und Logik in motor.cpp

int stallLightMinutes = 1;   // in light.cpp referenziert

// ==========================
// SETUP
// ==========================
void setup()
{

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
    loadBlockadeSettings();

    // ===== GPIO =====
    pinMode(MOTOR_IN1,          OUTPUT); digitalWrite(MOTOR_IN1, LOW);
    pinMode(MOTOR_IN2,          OUTPUT); digitalWrite(MOTOR_IN2, LOW);
    pinMode(RELAIS_PIN,         OUTPUT); digitalWrite(RELAIS_PIN, RELAY_OFF);
    pinMode(STALLLIGHT_RELAY_PIN,OUTPUT);digitalWrite(STALLLIGHT_RELAY_PIN, RELAY_OFF);
    pinMode(BUTTON_PIN,         INPUT_PULLUP);
    pinMode(STALL_BUTTON_PIN,   INPUT_PULLUP);
    pinMode(RED_BUTTON_PIN,     INPUT_PULLUP);
    pinMode(LIMIT_OPEN_PIN,     INPUT_PULLUP);
    pinMode(LIMIT_CLOSE_PIN,    INPUT_PULLUP);

    // ===== RGB + MOTOR =====
    lightInit();
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
    server.on("/blockade",   HTTP_GET,  handleBlockade);
    server.on("/save-blockade", HTTP_POST, []() {
        if (otaInProgress || ioSafeState) { server.send(503, "text/plain", "OTA aktiv"); return; }
        blockadeEnabled    = server.arg("enabled")   == "1";
        blockadeThresholdA = server.arg("threshold").toFloat();
        if (blockadeThresholdA < 0.5f || blockadeThresholdA > 10.0f) blockadeThresholdA = 2.0f;
        saveBlockadeSettings();
        addLog(String("Blockade: ") + (blockadeEnabled ? "aktiv" : "deaktiviert") +
               ", Schwelle=" + String(blockadeThresholdA, 1) + "A");
        server.send(200, "text/plain", "OK");
    });
    server.on("/blockade-live", HTTP_GET, []() {
        // 100 Samples für stabilen Mittelwert (ESP32 ADC rauscht stark)
        const int S = 100; long sum = 0;
        for (int i = 0; i < S; i++) sum += analogRead(ACS712_PIN);
        float vMeas   = (sum / S) * (3.3f / 4095.0f);
#if ACS712_HAS_DIVIDER
        float vSensor = vMeas / (20.0f / 30.0f);
#else
        float vSensor = vMeas;
#endif
        float amps    = fabsf((vSensor - ACS712_ZERO_V) / (ACS712_MV_PER_A / 1000.0f));
        // Gleitender Mittelwert → Display ruhig halten
        static float filtered = 0.0f;
        static bool  firstRun = true;
        if (firstRun) { filtered = amps; firstRun = false; }
        else          { filtered = filtered * 0.6f + amps * 0.4f; }
        // Sensor-Warnung NUR im Stillstand – beim laufenden Motor immer Rohwert zeigen
        if (filtered > 8.0f && motorState == MOTOR_STOPPED) {
            server.send(200, "text/plain", "-- (kein Sensor?)");
            return;
        }
        server.send(200, "text/plain", String(filtered, 2));
    });
    server.on("/blockade-peak", HTTP_GET, []() {
        server.send(200, "text/plain", String(peakCurrentA, 2));
    });
    server.on("/blockade-peak-reset", HTTP_POST, []() {
        peakCurrentA = 0.0f;
        server.send(200, "text/plain", "OK");
    });
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

    server.on("/rgbred", []() {
        if (rgbRedActive) { rgbRedOff(); server.send(200, "text/plain", "OFF"); }
        else { rgbRedOn(); server.send(200, "text/plain", "ON"); }
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
        JsonDocument doc;
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
            bool ok = !Update.hasError() && Update.end(true);
            Serial.printf("OTA end: ok=%d error=%d\n", ok, Update.getError());
            if (!ok) Update.printError(Serial);
            server.send(ok ? 200 : 500, "text/plain; charset=UTF-8", ok ? "Update erfolgreich" : "Update fehlgeschlagen");
            otaInProgress = false;
            ioSafeState   = false;
            if (ok) { delay(300); ESP.restart(); }
        },
        []() {
            HTTPUpload &u = server.upload();
            if (u.status == UPLOAD_FILE_START) {
                // Nur das Nötigste – kein MQTT, kein Log, kein Blocking!
                otaInProgress = true;
                ioSafeState   = true;
                motorStop();
                digitalWrite(MOTOR_IN1, LOW);
                digitalWrite(MOTOR_IN2, LOW);
                lightOff();
                stallLightOff();
                Serial.printf("OTA start: %s\n", u.filename.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
            }
            else if (u.status == UPLOAD_FILE_WRITE) {
                if (Update.write(u.buf, u.currentSize) != u.currentSize)
                    Update.printError(Serial);
            }
            else if (u.status == UPLOAD_FILE_END) {
                Serial.printf("OTA upload done: %d bytes\n", u.totalSize);
            }
            else if (u.status == UPLOAD_FILE_ABORTED) {
                Serial.println("OTA aborted");
                Update.abort();
                otaInProgress = false;
                ioSafeState   = false;
            }
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
    // WebServer bei jedem Durchlauf bedienen
    server.handleClient();

    // Während OTA-Upload: nur WebServer, kein delay, keine Logik
    if (otaInProgress) return;

    const unsigned long nowMs = millis();
    if (nowMs - lastLogicRun < LOGIC_INTERVAL) { delay(1); return; }
    lastLogicRun = nowMs;

    // ===== NETZWERK =====
    mqttLoop();
    wifiWatchdog();

    // ===== MOTOR + TASTER =====
    updateMotor();
    updateButton();
    updateStallButton();
    updateRedButton();

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
