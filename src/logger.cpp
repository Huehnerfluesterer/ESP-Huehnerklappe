#include "logger.h"
#include "system.h"   // rtc, rtcOk
#include <RTClib.h>

// ==================================================
// GLOBALE VARIABLEN (dieses Moduls)
// ==================================================
String logbook[LOG_SIZE];
int    logIndex = 0;

// Forward-Deklaration – mqttPublishLog kommt aus mqtt.cpp
void mqttPublishLog(const String &line);
extern bool mqttClientConnected(); // aus mqtt.cpp

// ==================================================
void addLog(const String &text)
{
    String ts;
    if (rtcOk)
    {
        DateTime now = rtc.now();
        char buf[20];
        snprintf(buf, sizeof(buf), "%02d.%02d.%04d %02d:%02d",
                 now.day(), now.month(), now.year(),
                 now.hour(), now.minute());
        ts = String(buf);
    }
    else
    {
        ts = String(millis() / 1000) + "s";
    }

    char entry[140];
    snprintf(entry, sizeof(entry), "%s – %s", ts.c_str(), text.c_str());

    logbook[logIndex] = String(entry);
    logIndex = (logIndex + 1) % LOG_SIZE;

    serialLog("📜 LOG: " + String(entry));

    if (mqttClientConnected())
        mqttPublishLog(String(entry));
}

void addLogWithLux(const String &text, float lx)
{
    String suffix = (isfinite(lx) && lx >= 0.0f && lx <= 120000.0f)
                  ? " – " + String(lx, 1) + " lx"
                  : " – n/a lx";
    addLog(text + suffix);
}

void clearLogbook()
{
    for (int i = 0; i < LOG_SIZE; i++) logbook[i] = "";
    logIndex = 0;
    Serial.println("📜 LOG: Logbuch gelöscht");
}

void serialLog(const String &msg)
{
    Serial.println(msg);
}

String buildLogHTML()
{
    String out;
    for (int i = 0; i < LOG_SIZE; i++)
    {
        int idx = (logIndex + i) % LOG_SIZE;
        if (logbook[idx].length() == 0) continue;

        String css = "log-entry";
        if (logbook[idx].indexOf("ERROR") >= 0) css += " error";
        else if (logbook[idx].indexOf("WARN") >= 0) css += " warn";

        out += "<div class='" + css + "'>" + logbook[idx] + "</div>";
    }
    if (out.length() == 0)
        out = "<div class='log-entry'>Keine Einträge vorhanden</div>";
    return out;
}
