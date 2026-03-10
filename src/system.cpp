#include "system.h"
#include "lux.h"
#include "light.h"
#include "motor.h"
#include "logger.h"
#include "pins.h"
#include <WiFi.h>
#include <Arduino.h>

// Forward-Deklarationen aus mqtt.cpp
void mqttPublishAvailability(const char *state);
bool mqttClientConnected();

// ==================================================
// GLOBALE VARIABLEN
// ==================================================
RTC_DS3231        rtc;
bool              rtcOk = false;

volatile bool otaInProgress = false;
volatile bool ioSafeState   = false;

bool errorWifi      = false;
bool errorMQTT      = false;
bool errorSensor    = false;   // true wenn VEML7700 ausgefallen

// ==================================================
bool systemError()
{
    return errorWifi || errorMQTT || errorSensor || !rtcOk;
}

void updateSystemHealth()
{
    errorWifi   = (WiFi.status() != WL_CONNECTED);
    errorSensor = (!hasVEML || vemlHardError);
    // errorMQTT wird direkt von mqtt.cpp gesetzt
}

// ==================================================
// OTA SAFE STATE
// ==================================================
void enterIoSafeState()
{
    otaInProgress = true;
    ioSafeState   = true;

    motorStop();
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    if (MOTOR_CH >= 0) ledcWrite(MOTOR_CH, 0);

    lightOff();
    stallLightOff();
    digitalWrite(RELAIS_PIN,          RELAY_OFF);
    digitalWrite(STALLLIGHT_RELAY_PIN, RELAY_OFF);

    lightActive      = false;
    manualLightActive = false;

    if (mqttClientConnected()) mqttPublishAvailability("offline");
    addLog("🛡️ I/O in sicheren Zustand versetzt (OTA)");
}

void leaveIoSafeState()
{
    ioSafeState = false;
    if (mqttClientConnected()) mqttPublishAvailability("online");
    addLog("✅ I/O Safe-State beendet");
}
