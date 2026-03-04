// =======================
// FIRMWARE VERSION
// =======================
#define FW_VERSION "1.0.10"  // WebUI Modifiziert GitHub - Umbau VEML7700


// ==================================================
// INCLUDE-BEREICH
// Beschreibung:
//   Einbindung aller benötigten Bibliotheken für
//   WLAN, Webserver, RTC, EEPROM, Lichtsensor,
//   OTA-Update, JSON-Verarbeitung und VEML7700.
// ==================================================
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <WebServer.h>
#include <RTClib.h>
#include <EEPROM.h>
// #include <BH1750.h>
#include <Adafruit_VEML7700.h>
#include <time.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <math.h>    // für isfinite()
#include <Update.h>  // Für Web-Upload OTA
#include <pgmspace.h>
#include <PubSubClient.h>  // MQTT
#include "SPIFFS.h"
#include <esp_task_wdt.h>
#include "config.h"


// ===== Funktions-Prototypen =====
void i2cBusRecover();   // I2C-Bus-Recovery bei blockiertem Sensor
void reinitVEML7700();  // Neuinitialisierung des VEML7700 (z.B. nach Fehlern)
float getLux();         // Lux-Wert von VEML7700 lesen (mit Fehlerbehandlung)
void autoRangeVEML(float luxValue); // Automatische Anpassung von Gain und Integration Time
// ===== Vorwärts-Deklarationen für Health =====
extern PubSubClient mqttClient;
extern float lux;
// BH1750 lightMeter;
Adafruit_VEML7700 veml; // VEML7700 Lichtsensor
/* ===================== PWA ICONS ===================== */
// 192x192 PNG
const unsigned char icon192_png[] PROGMEM = {
  137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 192, 0, 0, 0, 192, 8, 6, 0, 0, 0, 82, 220, 108, 7,
  0, 0, 4, 122, 73, 68, 65, 84, 120, 156, 237, 220, 81, 110, 212, 74, 16, 134, 209, 10, 98, 121, 65, 74, 86, 26, 164, 44,
  16, 30, 80, 16, 132, 36, 76, 198, 246, 184, 171, 254, 115, 94, 145, 174, 76, 87, 125, 118, 71, 92, 184, 187, 127, 122,
  248, 81, 16, 234, 203, 217, 15, 0, 103, 18, 0, 209, 4, 64, 52, 1, 16, 77, 0, 68, 19, 0, 209, 4, 64, 52, 1, 16, 77, 0, 68,
  19, 0, 209, 4, 64, 52, 1, 16, 77, 0, 68, 19, 0, 209, 4, 64, 52, 1, 16, 77, 0, 68, 19, 0, 209, 4, 64, 52, 1, 16, 77, 0, 68
};
const unsigned int icon192_png_len = 1146;

// 512x512 PNG
const unsigned char icon512_png[] PROGMEM = {
  137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 2, 0, 0, 0, 2, 0, 8, 6, 0, 0, 0, 244, 120, 212, 250,
  0, 0, 13, 140, 73, 68, 65, 84, 120, 156, 237, 220, 91, 110, 91, 55, 20, 64, 81, 165, 232, 240, 82, 32, 29, 105, 10, 116,
  128, 237, 71, 154, 34, 113, 252, 144, 116, 95, 36, 247, 90, 255, 45, 100, 217, 60, 103, 95, 202, 206, 167, 207, 95,
  191, 252, 115, 3, 0, 82, 126, 187, 250, 5, 0, 0, 231, 19, 0, 0, 16, 36, 0, 0, 32, 72, 0, 0, 64, 144, 0, 0, 128, 32, 1, 0,
  0, 65, 2, 0, 0, 130, 4, 0, 0, 4, 9, 0, 0, 8, 18, 0, 0, 16, 36, 0, 0, 32, 72, 0, 0, 64, 144, 0, 0, 128, 32, 1, 0, 0, 65, 2,
  0, 0, 13
};
const unsigned int icon512_png_len = 3442;

// --------------------------------------------------
// WS2812 LED-Konfiguration
// WS2812_PIN  : GPIO für LED-Datenleitung
// WS2812_COUNT: Anzahl LEDs im Streifen
// --------------------------------------------------
#define WS2812_PIN 4      // freier GPIO (nicht Boot-Pin!)
#define WS2812_COUNT 40   // Anzahl der LEDs im Streifen

/* ===================== FUNKTIONSPROTOTYPEN ===================== */
void handleRoot(); // WebUI: Hauptseite
void handleStatus(); // WebUI: Status-API
void handleSaveOpen(); // WebUI: Öffnungszeit speichern
void handleSaveClose(); // WebUI: Schließzeit speichern
void handleCalibration(); // WebUI: LDR-Kalibrierung
void handleLearn(); // WebUI: Einlernseite
void handleLearnPage(); // WebUI: Einlern-API
void addLog(String text);   //  Logbuch hinzufügen
void handleLogbook();     // WebUI: Logbuchseite     
void handleSettings();  // WebUI: Einstellungen speichern
void startLightForMinutes(int minutes);   // Locklicht für X Minuten aktivieren
void addLogWithLux(const String& text, float lx); //  Logbuch mit Lux-Wert
void startLightForMinutesReset(int minutes); // Locklicht für X Minuten aktivieren, Timer zurücksetzen wenn bereits aktiv
void updateButton(); // Taster-Status aktualisieren
void handleButtonPress();     // Taster-Interrupt: Tür öffnen/schließen
bool isManualAction(); // Prüft, ob die aktuelle Motoraktion manuell (Taster/Web) ausgelöst wurde
void handleSelftest();  // WebUI: Selbsttestseite
void handleAdvanced();  // WebUI: Erweiterte Einstellungen
void handleFw();  // WebUI: Firmware-Update (OTA)
void handleMqtt();      // WebUI: MQTT-Einstellungen speichern
void handleSaveMqtt();  // WebUI: MQTT-Einstellungen speichern
void serialLog(String msg); // Log-Ausgabe sowohl auf Serial als auch WebSerial
void motorStop();   //  Motor sofort stoppen                                          
void motorOpen();   //      Motor in Öffnungsrichtung starten
void motorClose();  //      Motor in Schließrichtung starten
void lightOff();      // Locklicht sofort ausschalten      
void stallLightOff();     // Stalllicht sofort ausschalten
void saveDoorState();     //  Türzustand (offen/geschlossen) im EEPROM speichern            
void loadDoorState();    //  Türzustand (offen/geschlossen) aus EEPROM laden


String renderThemeHead(String title);
String renderFooter();
void saveTheme(String theme);
/* ===================== LOGBUCH ===================== */
int timeToMinutes(String t);
void clearLogbook();
/* ===================== GLOBALE VARIABLEN ===================== */
Adafruit_NeoPixel stallLight(
  WS2812_COUNT,
  WS2812_PIN,
  NEO_GRB + NEO_KHZ800);


String uiTheme = "auto";  // auto | light | dark
// --------------------------------------------------
// OTA- und Sicherheitszustände
// otaInProgress : Firmware-Update läuft
// ioSafeState   : Peripherie in sicherem Zustand
// --------------------------------------------------
volatile bool otaInProgress = false;  // OTA läuft gerade
volatile bool ioSafeState = false;    // Peripherie im sicheren Zustand
// ===== Lux Trend Analyse =====
float luxHistory[20];     // Historie der letzten Lux-Werte für Trendberechnung
uint8_t luxIndex = 0;
unsigned long lastTrendUpdate = 0;

float luxTrend = 0;

// ===== Lux Median Filter =====
float luxMedianBuffer[5];
uint8_t luxMedianIndex = 0;
// ===== VEML7700 Auto Range =====
uint8_t vemlGain = VEML7700_GAIN_1_8;
uint8_t vemlIT = VEML7700_IT_25MS;

// ---- RTC/LEDC Guards ----
bool rtcOk = false;      // DS3231 betriebsbereit?
int  MOTOR_CH = -1;      // zugewiesener LEDC-Kanal (Core 3.x)

// ===== WS2812 Dimmen nach Schließen =====
bool dimmingActive = false;
unsigned long dimStartTime = 0;
unsigned long dimEndTime = 0;

bool useLimitSwitches = true;   // optional abschaltbar
unsigned long limitOpenSince = 0;   // Zeitstempel, seit dem der Öffnungs-Endschalter "offen" meldet
unsigned long limitCloseSince = 0;  // Zeitstempel, seit dem der Schließungs-Endschalter "geschlossen" meldet
const unsigned long LIMIT_DEBOUNCE_MS = 40;   // Entprellzeit für Endschalter (40 ms)

// --- Vor-Licht Hold-Logik (Morgen) ---
bool preLightHoldEnabled = true;                                  // bis zur Öffnung halten
unsigned long preLightStartedAt = 0;                              // Startzeit des Vor-Lichts
const unsigned long PRELIGHT_MAX_HOLD_MS = 45UL * 60UL * 1000UL;  // Sicherheitslimit: 45 Min


// --- Vor-Licht Hold-Logik (Abend) ---
bool preCloseHoldEnabled = true;
unsigned long preCloseStartedAt = 0;
const unsigned long PRECLOSE_MAX_HOLD_MS = 120UL * 60UL * 1000UL;

const float PRECLOSE_ABORT_POS_RATE = 0.25f;
const int PRECLOSE_ABORT_MARGIN_LX = 40;
const unsigned long PRECLOSE_ABORT_STABLE_MS = 180000UL;

// (optional)
// const unsigned long PRECLOSE_RESTART_COOLDOWN_MS = 300000UL;
// unsigned long lastPreCloseAbortAt = 0;
bool preLightOpenDone = false;
bool preLightCloseDone = false;
bool luxReady = false;

unsigned long lastDoorCloseTime = 0;

unsigned long manualOverrideUntil = 0;
String openMode = "time";
String closeMode = "time";
String openTime = "07:00";
String closeTime = "20:00";

int openLightThreshold = 300;
int closeLightThreshold = 200;

int lampPreOpen = 5;
int lampPostOpen = 5;
int lampPreClose = 5;
int lampPostClose = 5;

bool lightActive = false;
unsigned long lightOffTime = 0;

// ===== VEML Sensor-Überwachung =====
unsigned long lastVemlReinit = 0;
unsigned long lastLuxOkTime = 0;

const unsigned long VEML_REINIT_COOLDOWN = 15000;   // 15 Sekunden
const unsigned long VEML_HARD_TIMEOUT    = 60000;   // 60 Sekunden

bool vemlSoftError = false;
bool vemlHardError = false;

// ===== MANUELLES Locklicht =====
bool manualLightActive = false;
const int MANUAL_LIGHT_MINUTES = 15;

const int OPEN_FORECAST_TOLERANCE_MIN = 10;  // ±5 min Vor-Licht-Toleranz
const float MIN_POS_LUX_RATE = 0.15f;        // lx/min - Mindestanstieg damit Prognose sinnvoll
// const float MIN_POS_LUX_RATE = 0.3f;   // Öffnen
const float MIN_NEG_LUX_RATE = 0.15f;  // Schließen


// ===== STALLLICHT (manuell) =====
bool stallLightActive = false;
unsigned long stallLightOffTime = 0;

// Auto-Aus Dauer (Minuten)
int stallLightMinutes = 1;
int lastTimeActionMin = -1;

// ==================================================
// ENUM: DoorPhase
// Beschreibung:
//   Abbildung des aktuellen Türzustandes.
// ==================================================
enum DoorPhase {
  PHASE_IDLE,     // steht, wartet
  PHASE_OPENING,  // Motor öffnet
  PHASE_OPEN,     // offen
  PHASE_CLOSING   // Motor schließt
};
// ==================================================
// ENUM: MotorState
// Beschreibung:
//   Interner Motorlaufstatus.
// ==================================================
enum MotorState {
  MOTOR_STOPPED,
  MOTOR_OPENING,
  MOTOR_CLOSING
};

// --------------------------------------------------
// Globale Motorsteuerung
// motorState : aktueller Motorzustand
// motorUntil : Zeitstempel für automatisches Stoppen
// motorReason: Auslöser (Web, Licht, Zeit, Taster)
// --------------------------------------------------
MotorState motorState = MOTOR_STOPPED;
unsigned long motorUntil = 0;

// Wer den Motor gestartet hat (Web, Zeit, Licht, Taster)
String motorReason = "";

// ==================================================
// ENUM: LightState
// Beschreibung:
//   Zustandsmaschine für Locklicht.
// ==================================================
enum LightState {
  LIGHT_OFF,
  LIGHT_PRE_OPEN,
  LIGHT_POST_OPEN,
  LIGHT_PRE_CLOSE,
  LIGHT_POST_CLOSE,
  LIGHT_HOLD_CLOSE
};
// --------------------------------------------------
// Locklicht-Zustände
// lightState      : aktueller Lichtmodus
// lightStateUntil : Endzeitpunkt aktueller Phase
// lightActive     : Hardware-Zustand
// --------------------------------------------------
LightState lightState = LIGHT_OFF;
unsigned long lightStateUntil = 0;

DoorPhase doorPhase = PHASE_IDLE;

unsigned long bootTime = 0;

// ==================================================
// Lux-Trendberechnung
// Beschreibung:
//   Berechnet gefilterte Lux-Änderungsrate zur
//   Prognose von Sonnenauf- und Untergang.
// ==================================================
float lastLux = 0;
unsigned long lastLuxTime = 0;

float luxRateFiltered = 0;                            // geglättete Lux-Änderung (lx/min)
const float LUX_RATE_ALPHA = 0.2;                     // Glättung
const unsigned long LUX_TREND_INTERVAL_MS = 30000UL;  // 30 s
const int CLOSE_FORECAST_MAX_DISTANCE_LX = 300;
// ===== Trend-Stabilisierung (Schließen) =====
unsigned long closeBrightTrendSince = 0;
const unsigned long CLOSE_TREND_ABORT_MS = 90000UL;  // 90 Sekunden stabil heller

// ===== PRE-LIGHT STABILISIERUNG =====
bool preLightForecastCondition = false;
bool preLightForecastActive = false;
unsigned long preLightForecastSince = 0;

const unsigned long PRELIGHT_MIN_STABLE_MS = 90000UL;  // 60 Sekunden


/* ===================== EINLERNSTATUS ===================== */
bool learningActive = false;
bool learningOpenDone = false;
bool openedByLight = false;

unsigned long lightBelowSince = 0;
unsigned long lightAboveSince = 0;

// Entprellen / Stabilitätswartezeiten
const unsigned long LIGHT_DELAY_MS = 30000UL;       // Abend: 30 s dunkel am Stück
const unsigned long LIGHT_OPEN_DELAY_MS = 30000UL;  // Morgen: 30 s hell am Stück

unsigned long learnStartTime = 0;
unsigned long learnedOpenTime = 0;
unsigned long learnedCloseTime = 0;

unsigned long doorOpenedAt = 0;

// ===== NEU: Geplanter Schließ-Zeitpunkt nach Prognose =====
unsigned long plannedCloseAt = 0;

// ---- Fallback-Steuerung bei fehlendem Lichtsensor ----
bool    lightAutomationAvailable = false;     // aktuell nutzbarer Lichtwert?
unsigned long luxInvalidSince    = 0;         // seit wann sind die Lux-Werte ungültig?
const unsigned long LUX_FALLBACK_AFTER_MS = 5UL * 60UL * 1000UL; // nach 5 min ohne gültigen Lux → Zeit-Fallback erlauben
// ===== KONFIG: Mindest-Offen-Zeit vor Abendschließlogik =====
const unsigned long MIN_OPEN_BEFORE_CLOSE_MS = 15UL * 60UL * 1000UL;

// ===== Automatik-Zeitfenster (nur relevant bei openMode/closeMode == "light") =====
const int OPEN_WINDOW_START_H = 4;    // Öffnen erlaubt ab 05:00
const int OPEN_WINDOW_END_H = 14;     // Öffnen erlaubt bis 14:59
const int CLOSE_WINDOW_START_H = 15;  // Schließen erlaubt ab 15:00
const int CLOSE_WINDOW_END_H = 23;    // Schließen erlaubt bis 23:59

// --------------------------------------------------
// Nacht-Sperre
// Verhindert erneutes Öffnen nach Licht-Schließen,
// wenn beide Modi auf "light" stehen.
// --------------------------------------------------
bool nightLock = false;

// Schwellenbasierte Timer für "Vor-Licht" -> danach Aktion
unsigned long scheduledOpenAt = 0;
unsigned long scheduledCloseAt = 0;

// ===== Hysterese/Glitch-Filter ABEND (Schließen) =====
const int CLOSE_HYSTERESIS_LX = 10;            // Toleranz über Schließ-Schwelle
const unsigned long CLOSE_GLITCH_MS = 3000UL;  // bis 3 s kurz "heller" ignorieren
unsigned long closeInterruptionSince = 0; // Zeitpunkt, seit dem die Helligkeit über der Schließ-Schwelle war (für Glitch-Filter) 

// ===== Hysterese/Glitch-Filter MORGEN (Öffnen) =====
const int OPEN_HYSTERESIS_LX = 10;            // Toleranz unter Öffnen-Schwelle
const unsigned long OPEN_GLITCH_MS = 3000UL;  // bis 3 s kurz "dunkler" ignorieren
unsigned long openInterruptionSince = 0;

/* ===================== MQTT SETTINGS ===================== */

struct MqttSettings {
  bool enabled;
  char host[40];
  uint16_t port;
  char user[32];
  char pass[32];
  char clientId[32];
  char base[32];
};

MqttSettings mqttSettings;

/* ===================== SETTINGS ===================== */
struct Settings {
  char openMode[6];
  char closeMode[6];
  char openTime[6];
  char closeTime[6];

  int openLightThreshold;
  int closeLightThreshold;

  int lampPreOpen;
  int lampPostOpen;
  int lampPreClose;
  int lampPostClose;
};

Settings settings;

/* ===================== PINS ===================== */
#define MOTOR_IN1 25         // GPIO für Motorrichtung 1 (z.B. Öffnen)
#define MOTOR_IN2 26         // GPIO für Motorrichtung 2 (z.B. Schließen)
#define MOTOR_ENA 27         // GPIO für Motor-Enable (PWM)
#define RELAIS_PIN 18        // GPIO für Türrelais (LOW-aktiv)
#define BUTTON_PIN 33        //Taster für Klappe
#define STALL_BUTTON_PIN 32  //Taster für Stalllicht
#define LDR_PIN 35           // Lichtsensor (LDR) – analog
#define I2C_SDA 21           // I2C SDA (VEML7700, RTC)
#define I2C_SCL 22           // I2C SCL (VEML7700, RTC)
#define LIMIT_OPEN_PIN  14   // GPIO für Öffnungs-Endschalter (LOW = offen, HIGH = geschlossen)
#define LIMIT_CLOSE_PIN 12   // GPIO für Schließungs-Endschalter (LOW = geschlossen, HIGH = offen)

// ===== RELAIS-LOGIK (LOW-aktiv) =====
#define RELAY_ON LOW
#define RELAY_OFF HIGH

#define STALLLIGHT_RELAY_PIN 19

// ACS712 Stromsensor
#define ACS712_PIN 34
float currentBaseline = 0.0;
float currentThreshold = 0.0;
bool currentCalibrated = false;

/* ===================== STATUS ===================== */
bool doorOpen = false;

// ===== System Health =====
bool errorWifi = false;
bool errorMQTT = false;
bool hasVEML = false;
bool errorBH = false;

bool errorBHMissing = false;   // Sensor nicht vorhanden
bool errorBHTimeout = false;   // Sensor antwortet nicht
// ===== VEML7700 Health Monitoring =====
unsigned long vemlLastLux = 0;
// ===================== SYSTEM HEALTH =====================
void updateSystemHealth() {

  errorWifi = (WiFi.status() != WL_CONNECTED);
  errorMQTT = mqttSettings.enabled && !mqttClient.connected();

  // VEML Status
  errorBH = (!hasVEML || vemlHardError);
}

bool systemError() {
  bool rtcError = !rtcOk;
  return errorWifi || errorMQTT || errorBHMissing || errorBHTimeout || rtcError ;
}

String autoState = "Init";
long openPosition = 6000;
long closePosition = 6000;

int ldrMin = 4095;
int ldrMax = 0;

/* ===================== LOGBUCH ===================== */
#define LOG_SIZE 30
String logbook[LOG_SIZE];
int logIndex = 0;

/* ===================== RTC / WLAN ===================== */
RTC_DS3231 rtc;
WebServer server(80);


WiFiClient mqttWifi;
PubSubClient mqttClient(mqttWifi);

unsigned long mqttLastConnectAttempt = 0;
unsigned long mqttLastStatus = 0;
const unsigned long MQTT_STATUS_INTERVAL_MS = 10000UL;  // alle 10s

// Forward-Deklarationen
void mqttSetup();
void mqttLoop();
void mqttEnsureConnected();
void mqttSubscribeAll();
void mqttPublishAvailability(const char* state);
void mqttPublishStatus();
void mqttPublishSettings(bool retained = true);
void mqttPublishLog(const String& line);
void mqttCallback(char* topic, byte* payload, unsigned int length);
bool applySettingsFromJson(const JsonDocument& doc, String& err);


float lux = 0;
float luxMin = 999999;
float luxMax = 0;

unsigned long lastReinitTry = 0;

uint8_t luxFailCount = 0;

// ===== PROGNOSE-TESTMODUS =====
bool forecastTestMode = false;

// Virtueller Sonnenuntergang
float testLuxStart = 200.0;
float testLuxEnd = 5.0;
float testDurationMin = 20.0;
unsigned long testStartMillis = 0;

/* ===================== EEPROM ===================== */
#define EEPROM_SIZE 512
#define EEPROM_ADDR_DOORSTATE 400
#define EEPROM_ADDR_THEME 450
#define ADDR_USE_LIMIT_SWITCHES  120  // Adresse für die Speicherung der Endschalter-Nutzung (1 Byte: 0 = aus, 1 = an)

/* ===================== MOTOR ===================== */
void enterIoSafeState() {

    otaInProgress = true;
  
  ioSafeState = true;

  // Motor sofort stoppen, Richtungen low
  motorStop();
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  if (MOTOR_CH >= 0) ledcWrite(MOTOR_CH, 0);

  // Alle Lichter/Relais AUS
  lightOff();
  stallLightOff();
  digitalWrite(RELAIS_PIN, RELAY_OFF);
  digitalWrite(STALLLIGHT_RELAY_PIN, RELAY_OFF);

  // Zustände einfrieren, keine Auto-LL-Logik
  lightActive = false;
  manualLightActive = false;
  if (mqttClient.connected()) mqttPublishAvailability("offline");
  // Optional: WLAN-Client-Verbindungen offen lassen (OTA braucht Netzwerk)
  addLog("🛡️ I/O in sicheren Zustand versetzt (OTA)");
}

void leaveIoSafeState() {
  ioSafeState = false;
  if (mqttClient.connected()) mqttPublishAvailability("online");
  addLog("✅ I/O Safe-State beendet");
}

void reverseAfterBlockade() {
  Serial.println("↩️ Rückwärtsfahren nach Blockade");
  lightState = LIGHT_POST_OPEN;
  startLightForMinutes(lampPostOpen);
  motorOpen();
  delay(800);
  motorStop();
  lightState = LIGHT_POST_OPEN;
  startLightForMinutes(lampPostOpen);
}
// ==================================================
// FUNKTION: motorStop()
// Beschreibung:
//   Stoppt den Motor sofort.
//   Setzt beide Motor-Relais in LOW-Zustand.
//   Setzt internen Motorstatus auf MOTOR_STOPPED.
// Parameter:
//   keine
// Rückgabe:
//   keine
// ==================================================
void motorStop() {
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, LOW);
  // Hinweis: originaler ledcWrite/Attach bleibt wie im bestehenden Code
  if (MOTOR_CH >= 0) ledcWrite(MOTOR_CH, 0);
}
// ==================================================
// FUNKTION: motorOpen()
// Beschreibung:
//   Startet den Motor in Öffnungsrichtung.
//   Aktiviert OPEN-Relais und deaktiviert CLOSE.
//   Laufzeitbegrenzung erfolgt über motorUntil.
// Parameter:
//   durationMs – maximale Laufzeit in Millisekunden
//   reason     – Auslöser (Logging-Zwecke)
// Rückgabe:
//   keine
// ==================================================
void motorOpen() {
  digitalWrite(MOTOR_IN1, HIGH);
  digitalWrite(MOTOR_IN2, LOW);
  if (MOTOR_CH >= 0) ledcWrite(MOTOR_CH, 180);
}
// ==================================================
// FUNKTION: motorClose()
// Beschreibung:
//   Startet den Motor in Schließrichtung.
//   Aktiviert CLOSE-Relais und deaktiviert OPEN.
//   Laufzeitbegrenzung erfolgt über motorUntil.
// Parameter:
//   durationMs – maximale Laufzeit in Millisekunden
//   reason     – Auslöser (Logging-Zwecke)
// Rückgabe:
//   keine
// ==================================================
void motorClose() {
  digitalWrite(MOTOR_IN1, LOW);
  digitalWrite(MOTOR_IN2, HIGH);
  if (MOTOR_CH >= 0) ledcWrite(MOTOR_CH, 180);
}

void startMotorOpen(unsigned long durationMs) {
  motorOpen();
  motorState = MOTOR_OPENING;
  motorUntil = millis() + durationMs;
}

void startMotorClose(unsigned long durationMs) {
  motorClose();
  motorState = MOTOR_CLOSING;
  motorUntil = millis() + durationMs;
}
// ---- Erkennung manueller Aktionen (Taster/Web) -----------------------
inline bool isManualAction() {
  // deckt /door ("manuell/Web (Toggle)"), handleButtonPress ("manuell/Taster")
  // sowie generisch "Web" oder "Taster" ab
  return motorReason.indexOf("manuell") >= 0
         || motorReason.indexOf("Taster") >= 0
         || motorReason.indexOf("Web") >= 0;
}
void updateMotor() {

  if (motorState == MOTOR_STOPPED) return;

  // =====================================================
  // 🟢🔴 END-SCHALTER ÜBERWACHUNG (PRIORITÄT 1)
  // =====================================================

  // 🟢 Öffnen überwachen
  if (useLimitSwitches && motorState == MOTOR_OPENING) {

    if (digitalRead(LIMIT_OPEN_PIN) == LOW) {

      if (limitOpenSince == 0)
        limitOpenSince = millis();

      if (millis() - limitOpenSince > LIMIT_DEBOUNCE_MS) {

        motorStop();

        doorOpen = true;
        doorPhase = PHASE_OPEN;
        doorOpenedAt = millis();
        saveDoorState();

        addLog("Endschalter OBEN erreicht");

        motorState = MOTOR_STOPPED;
        limitOpenSince = 0;
        return;   // ganz wichtig
      }

    } else {
      limitOpenSince = 0;
    }
  }

  // 🔴 Schließen überwachen
  if (useLimitSwitches && motorState == MOTOR_CLOSING) {

    if (digitalRead(LIMIT_CLOSE_PIN) == LOW) {

      if (limitCloseSince == 0)
        limitCloseSince = millis();

      if (millis() - limitCloseSince > LIMIT_DEBOUNCE_MS) {

        motorStop();

        doorOpen = false;
        doorPhase = PHASE_IDLE;
        saveDoorState();

        addLog("Endschalter UNTEN erreicht");

        motorState = MOTOR_STOPPED;
        limitCloseSince = 0;
        return;   // ganz wichtig
      }

    } else {
      limitCloseSince = 0;
    }
  }

  // =====================================================
  // ORIGINALER TIMEOUT-BLOCK (unverändert)
  // =====================================================

  if (millis() >= motorUntil) {

    motorStop();

    if (motorState == MOTOR_OPENING) {

      doorOpen = true;
      doorPhase = PHASE_OPEN;
      doorOpenedAt = millis();
      saveDoorState();

      if (motorReason.indexOf("Licht") >= 0) {
        addLogWithLux("Klappe geöffnet (" + motorReason + ")", lux);
      } else {
        addLog("Klappe geöffnet (" + motorReason + ")");
      }

      preLightForecastActive = false;
      preLightForecastCondition = false;
      preLightStartedAt = 0;

      if (!isManualAction()) {
        lightState = LIGHT_POST_OPEN;
        startLightForMinutesReset(lampPostOpen);
        addLogWithLux("Locklicht nach Öffnung gestartet", lux);
      }

      lightBelowSince = 0;
      closeInterruptionSince = 0;
      plannedCloseAt = 0;

      motorReason = "";

    } else if (motorState == MOTOR_CLOSING) {

      doorOpen = false;
      doorPhase = PHASE_IDLE;
      saveDoorState();

      preLightOpenDone = false;
      preLightCloseDone = true;

      lastDoorCloseTime = millis();

      addLogWithLux("Klappe geschlossen (" + motorReason + ")", lux);

      if (openMode == "light" && closeMode == "light") {
        nightLock = true;
        addLog("Nacht-Sperre aktiv (durch Abschluss Schließen)");
      }

      if (!isManualAction()) {
        lightState = LIGHT_POST_CLOSE;
        startLightForMinutesReset(lampPostClose);
        lightActive = false;
        addLogWithLux("Locklicht nach Schließen gestartet", lux);

        if (lampPostClose >= 5) {
          dimEndTime = lightStateUntil;
          dimStartTime = dimEndTime - 5UL * 60000UL;
          dimmingActive = true;
        } else {
          dimmingActive = false;
        }
      }

      motorReason = "";
    }

    motorState = MOTOR_STOPPED;
  }
}

// ==================================================
// FUNKTION: lightOn()
// Beschreibung:
//   Aktiviert das Locklicht (WS2812 oder Relais).
//   Setzt internen Lichtstatus.
// Parameter:
//   keine
// Rückgabe:
//   keine
// ==================================================
void lightOn() {

  digitalWrite(RELAIS_PIN, RELAY_ON);  // 1:1 Spiegel: immer an wenn WS2812

  // 💡 WS2812 – warmweiß
  for (int i = 0; i < WS2812_COUNT; i++) {
    stallLight.setPixelColor(i, stallLight.Color(255, 180, 60));
  }
  stallLight.show();

  serialLog("💡 LICHT EIN");
}
// ==================================================
// FUNKTION: lightOff()
// Beschreibung:
//   Deaktiviert das Locklicht.
//   Setzt internen Lichtstatus zurück.
// Parameter:
//   keine
// Rückgabe:
//   keine
// ==================================================
void lightOff() {
  digitalWrite(RELAIS_PIN, RELAY_OFF);
  stallLight.clear();
  stallLight.show();
  stallLight.setBrightness(255);
  dimmingActive = false;
  lightState = LIGHT_OFF;
  Serial.println("💡 LICHT AUS (Relais + WS2812)");
}

void stallLightOn() {
  stallLightActive = true;
  stallLightOffTime = millis() + (unsigned long)stallLightMinutes * 60000UL;
  digitalWrite(STALLLIGHT_RELAY_PIN, RELAY_ON);
  addLog("Stalllicht angeschaltet");
}

void stallLightOff() {
  stallLightActive = false;
  digitalWrite(STALLLIGHT_RELAY_PIN, RELAY_OFF);
  addLog("Stalllicht ausgeschaltet");
}

void updateLightState() {


  if (manualLightActive) {
    // NIEMALS überschreiben, wenn manuell aktiv
    return;
  }
  unsigned long now = millis();
  switch (lightState) {
    case LIGHT_OFF:
      if (lightActive) {
        lightOff();
        lightActive = false;
        manualLightActive = false;  // manueller Modus Ende
      }
      break;

    case LIGHT_PRE_OPEN:
    case LIGHT_POST_OPEN:
    case LIGHT_PRE_CLOSE:
    case LIGHT_POST_CLOSE:
      if (!lightActive) {
        lightOn();
        lightActive = true;
      }


      if (now >= lightStateUntil) {

        if (lightState == LIGHT_POST_OPEN) {
          addLog("Locklicht nach Öffnung beendet");
        } else if (lightState == LIGHT_POST_CLOSE) {
          addLog("Locklicht nach Schließen beendet");
        }

        lightState = LIGHT_OFF;
      }

      break;
  }
}

void startLightForMinutes(int minutes) {
  unsigned long addMs;

  if (minutes <= 0) {
    addMs = 10000UL;  // Fallback 10 Sekunden
  } else {
    addMs = (unsigned long)minutes * 60000UL;
  }

  unsigned long now = millis();

  // Wenn Licht schon läuft → Zeit ANHÄNGEN
  if (lightStateUntil > now) {
    lightStateUntil += addMs;
  }
  // sonst neu starten
  else {
    lightStateUntil = now + addMs;
  }
}

// ✅ Setzt die Laufzeit IMMER neu (kein Anhängen) – ideal für POST_CLOSE
void startLightForMinutesReset(int minutes) {
  unsigned long addMs =
    (minutes <= 0) ? 10000UL : (unsigned long)minutes * 60000UL;
  lightStateUntil = millis() + addMs;
}

// ===== Hardware-Taster =====
// Debounce & Flankenerkennung (LOW = gedrückt, weil INPUT_PULLUP)
void updateButton() {
  static int lastRaw = HIGH;             // letzter ungefilterter Pegel
  static int stable = HIGH;              // aktueller entprellter Pegel
  static unsigned long lastChange = 0;   // Zeit der letzten Roh-Änderung
  const unsigned long DEBOUNCE_MS = 30;  // Entprellzeit

  int raw = digitalRead(BUTTON_PIN);

  if (raw != lastRaw) {  // Änderung erkannt -> Debounce starten
    lastChange = millis();
    lastRaw = raw;
  }

  // nach Ablauf der Entprellzeit den Pegel übernehmen
  if ((millis() - lastChange) > DEBOUNCE_MS && raw != stable) {
    stable = raw;
    if (stable == LOW) {  // FALLING-EDGE: Taste gedrückt
      // Boot-Schutz: erste 500ms ignorieren
      if (millis() - bootTime > 500) {
        handleButtonPress();
      }
    }
  }
}

void updateStallButton() {
  static int lastRaw = HIGH;
  static int stable = HIGH;
  static unsigned long lastChange = 0;
  const unsigned long DEBOUNCE_MS = 30;

  int raw = digitalRead(STALL_BUTTON_PIN);

  if (raw != lastRaw) {
    lastChange = millis();
    lastRaw = raw;
  }

  if ((millis() - lastChange) > DEBOUNCE_MS && raw != stable) {
    stable = raw;
    if (stable == LOW) {
      if (millis() - bootTime > 500) {

        if (stallLightActive) {
          stallLightOff();
        } else {
          stallLightOn();
        }
      }
    }
  }
}

void handleButtonPress() {
  // ===== EINLERNMODUS =====
  if (learningActive) {

    if (!learningOpenDone) {
      // Erste Position speichern
      learnedOpenTime = millis() - learnStartTime;
      openPosition = learnedOpenTime;

      EEPROM.put(100, openPosition);
      EEPROM.commit();

      learningOpenDone = true;
      addLog("Open-Position gespeichert: " + String(openPosition));

      // Jetzt in Gegenrichtung fahren
      learnStartTime = millis();
      motorClose();
      return;
    } else {
      // Zweite Position speichern
      learnedCloseTime = millis() - learnStartTime;
      closePosition = learnedCloseTime;

      EEPROM.put(104, closePosition);
      EEPROM.commit();

      learningActive = false;

      motorStop();
      addLog("Close-Position gespeichert: " + String(closePosition));
      addLog("Einlernen abgeschlossen");

      return;
    }
  }

  if (otaInProgress || ioSafeState) {
    addLog("Taster ignoriert (OTA/Safe-State)");
    return;
  }

  // ==================================================
  // ABSCHNITT: Motor-Laufzeitüberwachung
  // Beschreibung:
  //   Überprüft zyklisch, ob die maximale Motorlaufzeit
  //   überschritten wurde. Stoppt Motor automatisch.
  // ==================================================
  if (motorState != MOTOR_STOPPED) {
    motorStop();
    motorReason = "Stop/Taster";
    doorPhase = doorOpen ? PHASE_OPEN : PHASE_IDLE;  // Zustand konsistent halten
    addLog("Motor per Taster gestoppt");
    return;
  }

  // Motor steht: togglen wie /door
  if (doorOpen) {
    doorPhase = PHASE_CLOSING;
    motorReason = "manuell/Taster";
    startMotorClose(closePosition);
    preLightOpenDone = false;
    manualOverrideUntil = millis() + 300000UL;
    addLog("Schließvorgang gestartet (Taster)");
  } else {
    doorPhase = PHASE_OPENING;
    motorReason = "manuell/Taster";
    startMotorOpen(openPosition);
    preLightCloseDone = false;
    preLightOpenDone = false;
    manualOverrideUntil = millis() + 300000UL;
    addLog("Öffnung gestartet (Taster)");
  }
}
/* ===================== ACS712 ===================== */
float readMotorCurrent() {
  const int samples = 10;
  float sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(ACS712_PIN);
    delayMicroseconds(200);
  }
  float adc = sum / samples;
  float voltage = adc * (3.3 / 4095.0);
  return voltage;
}

/* ===================== KALIBRIERUNG ===================== */
void calibrateMotorCurrent() {
  Serial.println("🔧 Starte Strom-Kalibrierung...");
  if (motorReason.indexOf("Taster") == -1) {
    lightState = LIGHT_PRE_OPEN;
    startLightForMinutes(lampPreOpen);
  }
  startLightForMinutes(lampPreOpen);

  motorOpen();
  delay(1000);

  float sum = 0;
  const int samples = 50;
  for (int i = 0; i < samples; i++) {
    sum += readMotorCurrent();
    delay(20);
  }

  motorStop();

  currentBaseline = sum / samples;
  currentThreshold = currentBaseline + 0.4;
  currentCalibrated = true;

  Serial.print("✅ Ruhestrom: ");
  Serial.println(currentBaseline, 3);
  Serial.print("⚠️ Blockade-Schwelle: ");
  Serial.println(currentThreshold, 3);
}

/* ===================== EEPROM ===================== */
void saveSettings() {
  EEPROM.put(0, settings);
  EEPROM.commit();
}

void loadSettings() {
  memset(&settings, 0, sizeof(settings));
  EEPROM.get(0, settings);

  if (strlen(settings.openMode) == 0) {
    strcpy(settings.openMode, "time");
    strcpy(settings.closeMode, "time");
    strcpy(settings.openTime, "07:00");
    strcpy(settings.closeTime, "20:00");

    settings.openLightThreshold = 300;
    settings.closeLightThreshold = 200;

    settings.lampPreOpen = 5;
    settings.lampPostOpen = 5;
    settings.lampPreClose = 5;
    settings.lampPostClose = 5;

    saveSettings();
  }
}
void loadMqttSettings() {

  EEPROM.get(200, mqttSettings);

  if (strlen(mqttSettings.host) == 0) {
    mqttSettings.enabled = false;
    strcpy(mqttSettings.host, "0.0.0.0");
    mqttSettings.port = 1883;
    strcpy(mqttSettings.user, "");
    strcpy(mqttSettings.pass, "");
    strcpy(mqttSettings.clientId, "Chickendoor-ESP32");
    strcpy(mqttSettings.base, "Chickendoor");

    EEPROM.put(200, mqttSettings);
    EEPROM.commit();
  }
}

void saveMqttSettings() {
  EEPROM.put(200, mqttSettings);
  EEPROM.commit();
}
void saveDoorState() {
  EEPROM.put(EEPROM_ADDR_DOORSTATE, doorOpen);
  EEPROM.commit();
}

void loadDoorState() {
  EEPROM.get(EEPROM_ADDR_DOORSTATE, doorOpen);
  if (doorOpen != true && doorOpen != false) {
    doorOpen = false;
    doorPhase = PHASE_IDLE;
    saveDoorState();
  }
}
// ===================== WIFI HELFER =====================
unsigned long lastWifiCheck = 0;
unsigned long wifiBackoffMs = 0;  // einfacher Backoff
unsigned long lastReconnectTry = 0;

void wifiConnectNonBlocking() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  // Nicht ins Flash schreiben, vermeidet Wear
  WiFi.persistent(false);
  // Autoreconnect ist auf ESP32 verfügbar
  WiFi.setAutoReconnect(true);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  addLog("🔌 WLAN: Verbinde ...");
}

void wifiHardResetStack() {
  addLog("♻️ WLAN-Stack Neustart");
  WiFi.disconnect(true, true);  // trenne + vergiss
  WiFi.mode(WIFI_OFF);
  delay(100);
  wifiConnectNonBlocking();
}

void wifiWatchdog() {

  unsigned long now = millis();

  // nur alle 5s prüfen
  if (now - lastWifiCheck < 5000) return;
  lastWifiCheck = now;

  wl_status_t st = WiFi.status();
  if (st == WL_CONNECTED) {
    // Verbindung steht → Backoff zurücksetzen, optional RSSI loggen
    wifiBackoffMs = 0;
    // Serial.printf("WiFi OK, IP: %s RSSI: %d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    return;
  }

  // hier: nicht verbunden
  // Backoff anheben: 5s, 10s, 20s, max 60s
  if (wifiBackoffMs == 0) wifiBackoffMs = 5000;
  else wifiBackoffMs = min(wifiBackoffMs * 2, (unsigned long)60000);

  if (now - lastReconnectTry >= wifiBackoffMs) {
    lastReconnectTry = now;

    // 1) softer Versuch
    addLog("📡 WLAN weg – versuche Reconnect");
    WiFi.disconnect(false, false);
    WiFi.reconnect();

    // 2) Falls das mehrfach gar nichts bringt → harten Stack-Reset
    static uint8_t hardTries = 0;
    hardTries++;
    if (hardTries >= 3) {
      hardTries = 0;
      wifiHardResetStack();
    }
  }
}

// ==================================================
// FUNKTION: setup()
// Beschreibung:
//   Initialisierung aller Hardware-Komponenten.
//   WLAN, RTC, Lichtsensor, Webserver, OTA,
//   GPIO-Konfiguration, EEPROM-Laden.
// Parameter:
//   keine
// Rückgabe:
//   keine
// ==================================================
void setup() {
// ===== Watchdog (20 Sekunden) =====
esp_task_wdt_config_t wdt_config = {
  .timeout_ms = 20000,              // 20 Sekunden
  .idle_core_mask = (1 << 0) | (1 << 1),  // beide Kerne überwachen
  .trigger_panic = true            // bei Timeout -> Reset
};

pinMode(LIMIT_OPEN_PIN, INPUT_PULLUP);
pinMode(LIMIT_CLOSE_PIN, INPUT_PULLUP);

esp_task_wdt_init(&wdt_config);
esp_task_wdt_add(NULL);  // aktuellen Task hinzufügen

  WiFi.setSleep(false);
  Serial.begin(115200);
  loadMqttSettings();
  mqttSetup();

  pinMode(STALLLIGHT_RELAY_PIN, OUTPUT);
  digitalWrite(STALLLIGHT_RELAY_PIN, RELAY_OFF);

  pinMode(WS2812_PIN, OUTPUT);
  digitalWrite(WS2812_PIN, LOW);
  delay(50);

  pinMode(MOTOR_IN1, OUTPUT);
  pinMode(MOTOR_IN2, OUTPUT);
  pinMode(RELAIS_PIN, OUTPUT);
  digitalWrite(RELAIS_PIN, RELAY_OFF);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(STALL_BUTTON_PIN, INPUT_PULLUP);

  stallLight.begin();
  stallLight.clear();
  stallLight.show();

  
// LEDC (Core 3.x): Kanal zuweisen und merken
MOTOR_CH = ledcAttach(MOTOR_ENA, 2000, 8);  // 2 kHz, 8 Bit
if (MOTOR_CH < 0) {
  Serial.println("❌ LEDC: Kein Kanal verfügbar");
}


Wire.begin(I2C_SDA, I2C_SCL);
// Deine 30 kHz bleiben – ist für lange Leitungen hilfreich
Wire.setClock(30000);
Wire.setTimeOut(50);


rtcOk = rtc.begin();
if (!rtcOk) {
  Serial.println("⚠️ RTC DS3231 nicht gefunden – fahre ohne RTC fort.");
}


// ---- VEML7700 init (fehlertolerant) ----
hasVEML = veml.begin();

if (!hasVEML) {
  Serial.println("⚠️ VEML7700 nicht gefunden – starte ohne Lichtsensor.");
  // Health-Flags setzen, aber System NICHT stoppen
errorBH = true; 

  // Optional: späterer Reinit-Versuch (siehe reinitVEML7700())
} else {
  // Für Außeneinsatz gegen Sättigung:
  veml.setGain(VEML7700_GAIN_1_8);
  veml.setIntegrationTime(VEML7700_IT_25MS);
  Serial.println("✅ VEML7700 initialisiert");
  errorBHMissing = false;
  errorBHTimeout = false;
  errorBH = false;
}


  WiFi.mode(WIFI_STA);
  WiFi.setHostname("Huehnerklappe-ESP32");
  wifiConnectNonBlocking();
  // NICHT warten – der Server kann auch ohne WLAN schon laufen


  // Automatische Sommerzeit (Europa/Mitteleuropa)
  configTzTime(
    "CET-1CEST,M3.5.0/2,M10.5.0/3",  // Mitteleuropäische Zeit mit DST
    "pool.ntp.org",
    "time.nist.gov");



struct tm timeinfo;
static bool rtcSynced = false;

if (!rtcSynced && getLocalTime(&timeinfo)) {
  if (rtcOk) {
    rtc.adjust(DateTime(
      timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
    Serial.println("✅ RTC EINMALIG per NTP synchronisiert");
  } else {
    Serial.println("ℹ️ NTP Zeit geholt (keine RTC vorhanden)");
  }
  rtcSynced = true;
} else {
  Serial.println("❌ NTP Zeit konnte nicht geholt werden");
}

  EEPROM.begin(EEPROM_SIZE);
// ===== Endschalter-Einstellung laden =====
EEPROM.get(ADDR_USE_LIMIT_SWITCHES, useLimitSwitches);

// Falls EEPROM leer (z.B. 255), Default setzen
if (useLimitSwitches != true && useLimitSwitches != false) {
  useLimitSwitches = true;
}
  char themeBuf[10];
  EEPROM.get(EEPROM_ADDR_THEME, themeBuf);

  if (strlen(themeBuf) == 0) {
    uiTheme = "auto";
  } else {
    uiTheme = String(themeBuf);
  }

  if (strlen(themeBuf) == 0) {
    uiTheme = "auto";
  } else {
    uiTheme = String(themeBuf);
  }
    // Einlernwerte laden
  EEPROM.get(100, openPosition);
  EEPROM.get(104, closePosition);
  if (openPosition < 1000 || openPosition > 20000) openPosition = 6000;
  if (closePosition < 1000 || closePosition > 20000) closePosition = 6000;

  loadDoorState();
  if (doorOpen) {
    doorPhase = PHASE_OPEN;
    doorOpenedAt = millis();
  } else {
    doorPhase = PHASE_IDLE;
  }

  preLightOpenDone = false;
  openedByLight = false;
  lightAboveSince = 0;
  lightBelowSince = 0;


  
  // Settings laden
  loadSettings();

  openMode = settings.openMode;
  closeMode = settings.closeMode;
  openTime = settings.openTime;
  closeTime = settings.closeTime;

  openLightThreshold = settings.openLightThreshold;
  closeLightThreshold = settings.closeLightThreshold;

  lampPreOpen = settings.lampPreOpen;
  lampPostOpen = settings.lampPostOpen;
  lampPreClose = settings.lampPreClose;
  lampPostClose = settings.lampPostClose;

  // Server Routen
  server.on("/log/clear", HTTP_POST, []() {
    // Optional: kleine „Sicherung“ – z. B. nicht während OTA
    if (otaInProgress || ioSafeState) {
      server.send(503, "text/plain; charset=UTF-8", "OTA aktiv – Aktion gesperrt");
      return;
    }
    clearLogbook();
    addLog("Logbuch manuell gelöscht");

    // Antwort (plain oder JSON – hier schlicht)
    server.send(200, "text/plain; charset=UTF-8", "OK");
  });
  server.on("/mini", []() {
  server.send(200, "text/plain; charset=UTF-8", "OK");
});
  server.on("/", handleRoot);
  server.on("/status", handleStatus);
  server.on("/learn", handleLearn);
  server.on("/calibration", handleCalibration);
  server.on("/learn-page", handleLearnPage);
  server.on("/learn-start", HTTP_POST, handleLearn);
  server.on("/log", handleLogbook);
  server.on("/advanced", HTTP_GET, handleAdvanced);
  server.on("/fw", HTTP_GET, handleFw);
  server.on("/mqtt", HTTP_GET, handleMqtt);
  server.on("/save-mqtt", HTTP_POST, handleSaveMqtt);
  server.on("/set-theme", HTTP_POST, []() {
    String t = server.arg("theme");
    if (t == "dark" || t == "light" || t == "auto") {
      saveTheme(t);
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/icon-512.png", HTTP_GET, []() {
    server.sendHeader("Content-Type", "image/png");
    server.send_P(200, "image/png", (PGM_P)icon512_png, icon512_png_len);
  });
  server.on("/mqtt-test", HTTP_POST, []() {
    String host = server.arg("host");
    int port = server.arg("port").toInt();
    String user = server.arg("user");
    String pass = server.arg("pass");
    String clientId = server.arg("clientId");

    WiFiClient testClient;
    PubSubClient testMqtt(testClient);

    testMqtt.setServer(host.c_str(), port);

    bool ok;

    if (user.length() > 0) {
      ok = testMqtt.connect(clientId.c_str(), user.c_str(), pass.c_str());
    } else {
      ok = testMqtt.connect(clientId.c_str());
    }

    if (ok) {
      testMqtt.disconnect();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(200, "text/plain", "FAIL");
    }
  });
  server.on("/cal-up", HTTP_POST, []() {
    if (motorState == MOTOR_STOPPED) {
      motorReason = "Kalibrierung";
      startMotorOpen(openPosition);
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/cal-down", HTTP_POST, []() {
    if (motorState == MOTOR_STOPPED) {
      motorReason = "Kalibrierung";
      startMotorClose(closePosition);
    }
    server.send(200, "text/plain", "OK");
  });

  server.on("/cal-stop", HTTP_POST, []() {
    motorStop();
    motorState = MOTOR_STOPPED;
    server.send(200, "text/plain", "OK");
  });

  server.on("/cal-save-open", HTTP_POST, []() {
    openPosition = millis() - learnStartTime;
    EEPROM.put(100, openPosition);
    EEPROM.commit();
    addLog("Kalibrierung: Offen gespeichert");
    server.send(200, "text/plain", "OK");
  });

  server.on("/cal-save-close", HTTP_POST, []() {
    closePosition = millis() - learnStartTime;
    EEPROM.put(104, closePosition);
    EEPROM.commit();
    addLog("Kalibrierung: Geschlossen gespeichert");
    server.send(200, "text/plain", "OK");
  });
  server.on("/manifest.json", HTTP_GET, []() {
    server.send(200, "application/json", R"JSON(
{
  "name": "Hühnerklappe",
  "short_name": "Klappe",
  "description": "ESP32 Hühnerklappensteuerung",
  "start_url": "/",
  "scope": "/",
  "display": "standalone",
  "background_color": "#4CAF50",
  "theme_color": "#4CAF50",
  "icons": [
    {
      "src": "/icon-192.png",
      "sizes": "192x192",
      "type": "image/png"
    },
    {
      "src": "/icon-512.png",
      "sizes": "512x512",
      "type": "image/png"
    }
  ]
}
)JSON");
  });

  server.on("/reset", HTTP_POST, []() {
    server.send(200, "text/plain", "Restarting");

    delay(500);     // Antwort noch sauber senden
    ESP.restart();  // ESP32 Neustart
  });

  server.on("/motor/up", []() {
    motorOpen();
    server.send(200, "text/plain", "OK");
  });

  server.on("/motor/down", []() {
    motorClose();
    server.send(200, "text/plain", "OK");
  });

  server.on("/learn-status", []() {
    String json = "{";
    json += "\"active\":" + String(learningActive ? "true" : "false") + ",";
    json += "\"phase\":" + String(learningOpenDone ? 2 : 1);
    json += "}";
    server.send(200, "application/json", json);
  });

  server.on("/motor/stop", []() {
    motorStop();
    server.send(200, "text/plain", "OK");
  });

  server.on("/calib-status", []() {
    String json = "{";
    json += "\"open\":" + String(openPosition) + ",";
    json += "\"close\":" + String(closePosition);
    json += "}";
    server.send(200, "application/json", json);
  });


  server.on("/icon-192.png", HTTP_GET, []() {
    server.sendHeader("Content-Type", "image/png");
    server.send_P(200, "image/png", (PGM_P)icon192_png, icon192_png_len);
  });
  server.on("/systemtest", HTTP_GET, handleSelftest);

  server.on("/systemtest-status", HTTP_GET, []() {
  updateSystemHealth();

  bool wifiOk = WiFi.status() == WL_CONNECTED;
  long rssi   = wifiOk ? WiFi.RSSI() : 0;

  bool mqttOk = mqttClient.connected();

  float luxNow = lux;
  bool bhOk = hasVEML && !vemlHardError;

  // 🔎 RTC-Status
  const char* rtcStatus = rtcOk ? "OK" : "Nicht gefunden / nicht initialisiert";

  StaticJsonDocument<320> doc;   // etwas größer, wegen neuer Felder
  doc["wifi"]   = wifiOk;
  doc["rssi"]   = rssi;
  doc["mqtt"]   = mqttOk;
  doc["lux"]    = bhOk ? luxNow : -1;
  doc["bhOk"]   = bhOk;

  doc["rtcOk"]     = rtcOk ? 1 : 0;
  doc["rtcStatus"] = rtcStatus;

  doc["heap"]   = ESP.getFreeHeap();
  doc["uptime"] = millis() / 1000;
  doc["useLimitSwitches"] = useLimitSwitches;

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
});

  server.on("/systemtest-motor", HTTP_POST, []() {
    if (motorState == MOTOR_STOPPED && !otaInProgress && !ioSafeState) {
      startMotorOpen(200);
      delay(250);
      motorStop();
    }
    server.send(200, "text/plain", "OK");
  });

  // Eine Taste für Öffnen/Schließen/Stop
  server.on("/door", []() {
    if (otaInProgress || ioSafeState) {
      server.send(503, "text/plain", "OTA aktiv – Motor gesperrt");
      return;
    }

    // Falls Motor läuft: STOP als höchste Priorität (Sicherheits-Stop)
    if (motorState != MOTOR_STOPPED) {
      motorStop();
      motorReason = "Stop/Manuell";
      doorPhase = doorOpen ? PHASE_OPEN : PHASE_IDLE;  // Zustand konsistent halten
      server.send(200, "text/plain", "STOP");
      addLog("Motor gestoppt (Stopp-Taste)");
      return;
    }

    // Motor steht: togglen je nach aktuellem Türstatus
    if (doorOpen) {
      // Tür ist offen -> Schließen
      doorPhase = PHASE_CLOSING;
      motorReason = "manuell/Web (Toggle)";
      startMotorClose(closePosition);
      preLightOpenDone = false;
      manualOverrideUntil = millis() + 300000UL;
      server.send(200, "text/plain", "Closing");
      addLog("Schließvorgang gestartet (Toggle)");
    } else {
      // Tür ist zu -> Öffnen
      doorPhase = PHASE_OPENING;
      motorReason = "manuell/Web (Toggle)";
      startMotorOpen(openPosition);
      preLightCloseDone = false;
      preLightOpenDone = false;
      manualOverrideUntil = millis() + 300000UL;
      server.send(200, "text/plain", "Opening");
      addLog("Öffnung gestartet (Toggle)");
    }
  });

  // Upload/Flash (POST /update)
  server.on(
    "/update", HTTP_POST,
    // Abschluss-Handler (NACH dem Upload)
    []() {
      bool ok = Update.end(true);
      server.send(ok ? 200 : 500, "text/plain; charset=UTF-8", ok ? "Update erfolgreich" : "Update fehlgeschlagen");
      leaveIoSafeState();
      otaInProgress = false;
      if (ok) {
        delay(300);
        ESP.restart();
      }
    },
    // Upload-Stream-Handler (WÄHREND des Uploads)
    []() {
      if (!otaInProgress) {  // Guard einmalig setzen
        otaInProgress = true;
        enterIoSafeState();
      }
      HTTPUpload& upload = server.upload();

      if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("OTA START: %s\n", upload.filename.c_str());
        size_t sketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {  // oder: Update.begin();
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        esp_task_wdt_reset(); 
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        Serial.printf("OTA ENDE: %u Bytes\n", upload.totalSize);
      } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Update.abort();
        Serial.println("OTA ABGEBROCHEN");
        leaveIoSafeState();
        otaInProgress = false;
      }
    });
  // /open: nur EINE Antwort senden
  server.on("/open", []() {
    if (otaInProgress || ioSafeState) {
      server.send(503, "text/plain", "OTA aktiv – Motor gesperrt");
      return;
    }
    if (doorOpen) {
      server.send(200, "text/plain", "Already open");
      return;
    }
    if (motorState != MOTOR_STOPPED) {
      server.send(200, "text/plain", "Motor already running");
      return;
    }

    doorPhase = PHASE_OPENING;
    motorReason = "manuell/Web";
    startMotorOpen(openPosition);

    preLightCloseDone = false;
    preLightOpenDone = false;
    manualOverrideUntil = millis() + 300000UL;

    server.send(200, "text/plain", "Opening");
  });

  server.on("/test/forecast/start", HTTP_GET, []() {
    forecastTestMode = true;
    luxReady = true;
    testStartMillis = millis();

    lastLux = testLuxStart;
    lastLuxTime = millis();
    luxRateFiltered = 0;

    addLog("TEST: Prognose-Sonnenuntergang gestartet");
    server.send(200, "text/plain", "Prognose-Test gestartet");
  });

  server.on("/test/forecast/stop", HTTP_GET, []() {
    forecastTestMode = false;
    addLog("TEST: Prognose-Test beendet");
    server.send(200, "text/plain", "Prognose-Test beendet");
  });

  server.on("/close", []() {
    if (otaInProgress || ioSafeState) {
      server.send(503, "text/plain", "OTA aktiv – Motor gesperrt");
      return;
    }

    if (!doorOpen) {
      server.send(200, "text/plain", "Already closed");
      return;
    }
    if (motorState != MOTOR_STOPPED) {
      server.send(200, "text/plain", "Motor already running");
      return;
    }
    doorPhase = PHASE_CLOSING;
    motorReason = "manuell/Web";
    startMotorClose(closePosition);

    preLightOpenDone = false;
    manualOverrideUntil = millis() + 300000UL;

    server.send(200, "text/plain", "Closing");
  });

  // Manuelles Locklicht (Toggle)

  server.on("/light", []() {
    if (manualLightActive) {
      // manuelles Licht AUS
      manualLightActive = false;
      lightOff();
      lightActive = false;
      addLog("Locklicht manuell AUS");
      server.send(200, "text/plain", "OFF");
    } else {
      // manuelles Licht AN (dauerhaft)
      manualLightActive = true;
      lightOn();
      lightActive = true;
      addLog("Locklicht manuell AN");
      server.send(200, "text/plain", "ON");
    }
  });


  // Stalllicht-Relais direkt
  server.on("/stalllight", []() {
    if (stallLightActive) {
      stallLightOff();
      server.send(200, "text/plain", "OFF");
    } else {
      stallLightOn();
      server.send(200, "text/plain", "ON");
    }
  });

  server.on("/settings", handleSettings);
  server.on("/set-limit-switches", HTTP_POST, []() {

  if (server.hasArg("enabled")) {
    useLimitSwitches = server.arg("enabled") == "1";
    
    EEPROM.put(ADDR_USE_LIMIT_SWITCHES, useLimitSwitches);
    EEPROM.commit();

    addLog(String("Endschalter ") + (useLimitSwitches ? "aktiviert" : "deaktiviert"));
  }

  server.send(200, "text/plain", "OK");
});
  server.on("/save-open", HTTP_POST, handleSaveOpen);
  server.on("/save-close", HTTP_POST, handleSaveClose);

  openedByLight = false;
  lightAboveSince = 0;
  lightBelowSince = 0;

  Serial.println("Relais TEST: EIN");
  lightOn();
  delay(3000);
  Serial.println("Relais TEST: AUS");
  lightOff();
  delay(1000);

  bootTime = millis();  // Startzeit für Schutzfenster
  server.begin();
  // server.enableDelay(tru);
}
/* ===================== MQTT FUNKTIONEN ===================== */
inline String t(const char* sub) {
  return String(mqttSettings.base) + "/" + sub;
}

inline void mqttPublish(const String& topic, const String& payload, bool retained = false) {
  mqttClient.publish(topic.c_str(), payload.c_str(), retained);
}

inline void mqttPublishRaw(const String& topic, const uint8_t* data, size_t len, bool retained = false) {
  mqttClient.publish(topic.c_str(), data, len, retained);
}

void mqttPublishAvailability(const char* state) {
  mqttPublish(t("tele/availability"), state, true);  // retained
}

void mqttPublishSettings(bool retained) {
  StaticJsonDocument<512> doc;
  doc["openMode"] = openMode;
  doc["closeMode"] = closeMode;
  doc["openTime"] = openTime;
  doc["closeTime"] = closeTime;
  doc["openLightThreshold"] = openLightThreshold;
  doc["closeLightThreshold"] = closeLightThreshold;
  doc["lampPreOpen"] = lampPreOpen;
  doc["lampPostOpen"] = lampPostOpen;
  doc["lampPreClose"] = lampPreClose;
  doc["lampPostClose"] = lampPostClose;

  String out;
  serializeJson(doc, out);
  mqttPublish(t("tele/settings"), out, retained);
}

void mqttPublishStatus() {
  StaticJsonDocument<512> doc;

  DateTime nowDT = rtcOk ? rtc.now() : DateTime(2000, 1, 1, 0, 0, 0);
  String dateStr = rtcOk
      ? (String(nowDT.day()) + "." + String(nowDT.month()) + "." + String(nowDT.year()))
      : "-";

  doc["time"]       = rtcOk ? nowDT.timestamp(DateTime::TIMESTAMP_TIME) : "--:--:--";
  doc["date"]       = dateStr;
  doc["door"]       = doorOpen ? "Offen" : "Geschlossen";
  doc["moving"]     = (motorState != MOTOR_STOPPED) ? "1" : "0";
  doc["light"]      = isfinite(lux) ? String(lux, 1) : "n/a";
  doc["sensor"] = (hasVEML && !vemlHardError) ? "VEML7700 OK" : "Lichtsensor Fehler";
  doc["lightState"] = lightActive ? "An" : "Aus";
  doc["stallLight"] = stallLightActive ? "An" : "Aus";
  doc["fw"]         = FW_VERSION;

  String out;
  serializeJson(doc, out);
  mqttPublish(t("tele/status"), out);
}

void mqttPublishLog(const String& line) {
  mqttPublish(t("tele/log"), line);
}

bool applySettingsFromJson(const JsonDocument& doc, String& err) {
  Settings s = settings;  // Kopie

  if (doc.containsKey("openMode")) strncpy(s.openMode, doc["openMode"].as<const char*>(), 5), s.openMode[5] = '\0';
  if (doc.containsKey("closeMode")) strncpy(s.closeMode, doc["closeMode"].as<const char*>(), 5), s.closeMode[5] = '\0';
  if (doc.containsKey("openTime")) strncpy(s.openTime, doc["openTime"].as<const char*>(), 5), s.openTime[5] = '\0';
  if (doc.containsKey("closeTime")) strncpy(s.closeTime, doc["closeTime"].as<const char*>(), 5), s.closeTime[5] = '\0';

  if (doc.containsKey("openLightThreshold")) s.openLightThreshold = doc["openLightThreshold"].as<int>();
  if (doc.containsKey("closeLightThreshold")) s.closeLightThreshold = doc["closeLightThreshold"].as<int>();

  if (doc.containsKey("lampPreOpen")) s.lampPreOpen = doc["lampPreOpen"].as<int>();
  if (doc.containsKey("lampPostOpen")) s.lampPostOpen = doc["lampPostOpen"].as<int>();
  if (doc.containsKey("lampPreClose")) s.lampPreClose = doc["lampPreClose"].as<int>();
  if (doc.containsKey("lampPostClose")) s.lampPostClose = doc["lampPostClose"].as<int>();

  // gleiche Validierung wie HTTP-Handlers
  if (s.openLightThreshold <= s.closeLightThreshold) {
    err = "Fehler: Öffnen-Lux muss größer sein als Schließen-Lux!";
    return false;
  }

  // übernehmen + speichern
  settings = s;
  saveSettings();

  openMode = settings.openMode;
  closeMode = settings.closeMode;
  openTime = settings.openTime;
  closeTime = settings.closeTime;

  openLightThreshold = settings.openLightThreshold;
  closeLightThreshold = settings.closeLightThreshold;

  lampPreOpen = settings.lampPreOpen;
  lampPostOpen = settings.lampPostOpen;
  lampPreClose = settings.lampPreClose;
  lampPostClose = settings.lampPostClose;
  return true;
}

void mqttSubscribeAll() {
  mqttClient.subscribe(t("cmnd/#").c_str());  // alle Commands
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String top = String(topic);
  String pay;
  pay.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++) pay += (char)payload[i];

  String cmd = pay;
  cmd.trim();
  cmd.toUpperCase();

  // Tür-Steuerung
  if (top == t("cmnd/door")) {
    if (otaInProgress || ioSafeState) {
      addLog("MQTT: Motor gesperrt (OTA/Safe-State)");
      return;
    }
    if (cmd == "OPEN") {
      if (!doorOpen && motorState == MOTOR_STOPPED) {
        doorPhase = PHASE_OPENING;
        motorReason = "manuell/MQTT";
        startMotorOpen(openPosition);
        preLightCloseDone = false;
        preLightOpenDone = false;
        manualOverrideUntil = millis() + 300000UL;
        addLog("Öffnung gestartet (MQTT)");
      }
    } else if (cmd == "CLOSE") {
      if (doorOpen && motorState == MOTOR_STOPPED) {
        doorPhase = PHASE_CLOSING;
        motorReason = "manuell/MQTT";
        startMotorClose(closePosition);
        preLightOpenDone = false;
        manualOverrideUntil = millis() + 300000UL;
        addLog("Schließvorgang gestartet (MQTT)");
      }
    } else if (cmd == "STOP") {
      if (motorState != MOTOR_STOPPED) {
        motorStop();
        motorReason = "Stop/MQTT";
        doorPhase = doorOpen ? PHASE_OPEN : PHASE_IDLE;
        addLog("Motor gestoppt (MQTT)");
      }
    } else if (cmd == "TOGGLE") {
      if (motorState != MOTOR_STOPPED) {
        motorStop();
        motorReason = "Stop/MQTT";
        doorPhase = doorOpen ? PHASE_OPEN : PHASE_IDLE;
        addLog("Motor gestoppt (MQTT/Toggle)");
      } else if (doorOpen) {
        doorPhase = PHASE_CLOSING;
        motorReason = "manuell/MQTT";
        startMotorClose(closePosition);
        preLightOpenDone = false;
        manualOverrideUntil = millis() + 300000UL;
        addLog("Schließvorgang gestartet (MQTT/Toggle)");
      } else {
        doorPhase = PHASE_OPENING;
        motorReason = "manuell/MQTT";
        startMotorOpen(openPosition);
        preLightCloseDone = false;
        preLightOpenDone = false;
        manualOverrideUntil = millis() + 300000UL;
        addLog("Öffnung gestartet (MQTT/Toggle)");
      }
    }
    mqttPublishStatus();
    return;
  }

  // Locklicht
  if (top == t("cmnd/light")) {
    if (cmd == "ON") {
      manualLightActive = true;
      lightOn();
      lightActive = true;
      addLog("Locklicht manuell AN (MQTT)");
    }
    if (cmd == "OFF") {
      manualLightActive = false;
      lightOff();
      lightActive = false;
      addLog("Locklicht manuell AUS (MQTT)");
    }
    if (cmd == "TOGGLE") {
      if (manualLightActive) {
        manualLightActive = false;
        lightOff();
        lightActive = false;
        addLog("Locklicht manuell AUS (MQTT/Toggle)");
      } else {
        manualLightActive = true;
        lightOn();
        lightActive = true;
        addLog("Locklicht manuell AN (MQTT/Toggle)");
      }
    }
    mqttPublishStatus();
    return;
  }

  // Stalllicht
  if (top == t("cmnd/stalllight")) {
    if (cmd == "ON") stallLightOn();
    else if (cmd == "OFF") stallLightOff();
    else if (cmd == "TOGGLE") {
      if (stallLightActive) stallLightOff();
      else stallLightOn();
    }
    mqttPublishStatus();
    return;
  }

  // Settings setzen (JSON)
  if (top == t("cmnd/settings")) {
    StaticJsonDocument<512> d;
    DeserializationError e = deserializeJson(d, pay);
    if (e) {
      addLog(String("MQTT Settings JSON Fehler: ") + e.c_str());
      return;
    }
    String err;
    if (applySettingsFromJson(d, err)) {
      addLog("MQTT Settings übernommen");
      mqttPublishSettings(true);
      mqttPublishStatus();
    } else {
      addLog("MQTT Settings FEHLER: " + err);
    }
    return;
  }

  // GET: Status / Settings / Log
  if (top == t("cmnd/get")) {
    if (cmd == "STATUS") mqttPublishStatus();
    else if (cmd == "SETTINGS") mqttPublishSettings(true);
    else if (cmd == "LOG") {
      for (int i = 0; i < LOG_SIZE; i++) {
        int idx = (logIndex + i) % LOG_SIZE;
        if (logbook[idx].length() > 0) mqttPublishLog(logbook[idx]);
      }
    }
    return;
  }

  // Reboot
  if (top == t("cmnd/reboot") && cmd == "NOW") {
    addLog("MQTT: Reboot…");
    mqttPublishAvailability("offline");
    delay(100);
    ESP.restart();
  }
}

void mqttSetup() {
  mqttClient.setServer(mqttSettings.host, mqttSettings.port);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(1024);
  mqttClient.setKeepAlive(15);
  mqttClient.setSocketTimeout(3);
}

void mqttSubscribeAll();  // Vorwärtsreferenz (optional)
void mqttEnsureConnected() {
  if (mqttClient.connected()) return;

  unsigned long now = millis();
  if (now - mqttLastConnectAttempt < 3000UL) return;  // 3s Backoff
  mqttLastConnectAttempt = now;

  String willTopic = t("tele/availability");
  const char* willMsg = "offline";

  bool ok;

  if (strlen(mqttSettings.user) > 0) {
    ok = mqttClient.connect(
      mqttSettings.clientId,
      mqttSettings.user, mqttSettings.pass,
      willTopic.c_str(), /* will QoS */ 0, /* will retain */ true, /* will msg */ willMsg);
  } else {
    ok = mqttClient.connect(
      mqttSettings.clientId,
      willTopic.c_str(), /* will QoS */ 0, /* will retain */ true, /* will msg */ willMsg);
  }


  if (ok) {
    addLog("MQTT verbunden");
    mqttPublishAvailability("online");
    mqttSubscribeAll();
    mqttPublishSettings(true);
    mqttPublishStatus();
  } else {
    addLog("MQTT Verbindung fehlgeschlagen");
  }
}

void mqttLoop() {
  if (!mqttSettings.enabled) return;
  if (!WiFi.isConnected()) return;
  mqttEnsureConnected();
  if (!mqttClient.connected()) return;

  mqttClient.loop();

  unsigned long now = millis();
  if (now - mqttLastStatus >= MQTT_STATUS_INTERVAL_MS) {
    mqttLastStatus = now;
    mqttPublishStatus();
  }
}

/* ===================== LUX ===================== */

float getLux() {

  if (!hasVEML) return NAN;

  float value = veml.readLux();

  if (!isfinite(value) || value < 0 || value > 120000) {
    return NAN;
  }

  // automatische Empfindlichkeitsanpassung
  autoRangeVEML(value);

  return value;
}
void updateLuxTrend(float currentLux) {

  luxHistory[luxIndex] = currentLux;

  luxIndex = (luxIndex + 1) % 20;

  float oldest = luxHistory[luxIndex];

  if (oldest == 0) return;

  luxTrend = currentLux - oldest;
}
float medianLux(float newValue) {

  luxMedianBuffer[luxMedianIndex] = newValue;
  luxMedianIndex = (luxMedianIndex + 1) % 5;

  float temp[5];
  memcpy(temp, luxMedianBuffer, sizeof(temp));

  // simple sort
  for (int i = 0; i < 4; i++) {
    for (int j = i + 1; j < 5; j++) {
      if (temp[j] < temp[i]) {
        float t = temp[i];
        temp[i] = temp[j];
        temp[j] = t;
      }
    }
  }

  return temp[2];
}
bool predictSunrise() {

  static uint8_t stableRiseCounter = 0;

  if (luxTrend > 1.5 && luxRateFiltered > 0.008) {
    if (stableRiseCounter < 5)
      stableRiseCounter++;
  }
  else {
    stableRiseCounter = 0;
  }

  if (lux > 1 && stableRiseCounter >= 3) {
    stableRiseCounter = 3;   // Deckelung
    return true;
  }

  return false;
}
void reinitVEML7700() {

  if (millis() - lastVemlReinit < VEML_REINIT_COOLDOWN)
    return;

  lastVemlReinit = millis();

  Serial.println("🔄 Versuche VEML Reinit...");

  i2cBusRecover();

  if (veml.begin()) {
vemlGain = VEML7700_GAIN_1_8;
vemlIT   = VEML7700_IT_25MS;

veml.setGain(vemlGain);
veml.setIntegrationTime(vemlIT);

    hasVEML = true;
    luxFailCount = 0;

    vemlSoftError = false;
    vemlHardError = false;
    errorBH = false;

    Serial.println("✅ VEML erfolgreich reinitialisiert");
    addLog("VEML7700 reinitialisiert");

  } else {

    hasVEML = false;
    errorBH = true;

    Serial.println("❌ VEML Reinit fehlgeschlagen");
  }
}


// ==================================================
// FUNKTION: loop()
// Beschreibung:
//   Hauptzyklus des Systems.
//   Enthält:
//     - Webserver Handling
//     - Motorüberwachung
//     - Licht-Zustandsmaschine
//     - Lux-Analyse
//     - Automatik-Logik
//     - WLAN-Watchdog
// ==================================================
void loop() {
  server.handleClient();
  esp_task_wdt_reset();  
    float rawLux = NAN;
  bool luxValid = false;
  // Verbindungs-/HTTP- und Eingabelogik
  mqttLoop();
  wifiWatchdog();
    updateMotor();
  updateButton();
  updateStallButton();  // Stalllicht
  // ===== LUX LESEN (alle 500 ms) =====
static unsigned long lastLuxRead = 0;

if (millis() - lastLuxRead > 1000) { 
  lastLuxRead = millis();

  rawLux = getLux();
if (forecastTestMode) {

  float elapsedMin = (millis() - testStartMillis) / 60000.0;

  if (elapsedMin > testDurationMin)
    elapsedMin = testDurationMin;

  float progress = elapsedMin / testDurationMin;

  rawLux = testLuxStart - (testLuxStart - testLuxEnd) * progress;
}

  // ===== Medianfilter gegen Ausreißer =====
  if (isfinite(rawLux)) {
    rawLux = medianLux(rawLux);
  }

serialLog(
  "Lux=" + String(rawLux) +
  " Trend=" + String(luxTrend) +
  " Rate=" + String(luxRateFiltered) +
  " SunrisePredict=" + String(predictSunrise())
);
}

// ===== Validierung =====
luxValid = isfinite(rawLux) && rawLux >= 0.0f;

// ===== Fehlerüberwachung =====
if (!luxValid) {

  if (luxInvalidSince == 0)
    luxInvalidSince = millis();

  if (luxFailCount < 255)
    luxFailCount++;

} else {

  // gültiger Wert → alles zurücksetzen
  luxFailCount = 0;
  luxInvalidSince = 0;
  vemlSoftError = false;
  vemlHardError = false;
  errorBH = false;
}

// ===== SOFT ERROR (nur markieren, kein Reinit) =====
if (hasVEML && luxFailCount >= 3 && !vemlSoftError) {

  vemlSoftError = true;
  // Serial.println("⚠️ VEML Soft-Error");
}

// ===== HARD ERROR (60s kein gültiger Wert) =====
// ===== HARD ERROR =====
if (hasVEML && luxInvalidSince > 0 &&
    millis() - luxInvalidSince > VEML_HARD_TIMEOUT &&
    !vemlHardError) {

  vemlHardError = true;
  hasVEML = false;
  errorBH = true;

  Serial.println("❌ VEML Hard-Error");
  addLog("VEML7700 Hard-Error");

  if (millis() - lastVemlReinit > VEML_REINIT_COOLDOWN)
    reinitVEML7700();
}

// ===== Filter nur bei gültigem Wert =====
static float luxFiltered = 0;

if (luxValid) {

  vemlLastLux = millis();   // <<< NEU

  if (luxFiltered == 0)
    luxFiltered = rawLux;
  else
    luxFiltered = luxFiltered * 0.8f + rawLux * 0.2f;

  lux = luxFiltered;
}
lightAutomationAvailable = luxValid && hasVEML && !vemlHardError;
// ===== Lux Trend Update (alle 30 Sekunden) =====
if (luxValid && millis() - lastTrendUpdate > 30000) {

  updateLuxTrend(lux);

  lastTrendUpdate = millis();
}

  // Automatik-Sperre nach manueller Aktion
  const bool automatikErlaubt = (millis() >= manualOverrideUntil);

  updateSystemHealth();
  // BH1750 Initialisierung absichern
  if (!luxReady) {
    if (lux > 5) {
      luxReady = true;
      lastLux = lux;
      lastLuxTime = millis();
      luxRateFiltered = 0;
      Serial.println("✅ Lux-Sensor stabil (Trend gestartet)");
    }
  }

  // Zeit und Fenster berechnen
 // handleStatus()
DateTime now = rtcOk ? rtc.now() : DateTime(2000,1,1,0,0,0);
  const int nowMin = now.hour() * 60 + now.minute();

  // Nacht-Sperre morgens (einmal täglich ab 05:00) zurücksetzen
static int nightLockResetDay = -1;

if (openMode == "light" && closeMode == "light") {

  if (nightLock &&
    now.hour() >= OPEN_WINDOW_START_H &&
    now.hour() < CLOSE_WINDOW_START_H)
{
    if (now.day() != nightLockResetDay) {
        nightLock = false;
        nightLockResetDay = now.day();
        addLog("Nacht-Sperre zurückgesetzt");
    }
}

}
  // Fenster-Flags
  const bool inOpenWindow = (now.hour() >= OPEN_WINDOW_START_H && now.hour() <= OPEN_WINDOW_END_H);
  const bool inCloseWindow = (now.hour() >= CLOSE_WINDOW_START_H && now.hour() <= CLOSE_WINDOW_END_H);

  // ===== ZEITMODUS: LOCKLICHT VOR ÖFFNUNG =====
  if (automatikErlaubt && openMode == "time" && doorPhase == PHASE_IDLE && !doorOpen && !preLightOpenDone) {

    int openMin = timeToMinutes(openTime);
    int diffMin = openMin - nowMin;
    if (diffMin < 0) diffMin += 1440;

    if (diffMin > 0 && diffMin <= lampPreOpen && !preLightOpenDone) {
      lightState = LIGHT_PRE_OPEN;
      //startLightForMinutes(lampPreOpen);
      startLightForMinutesReset(lampPreOpen);  // <<<<<< exakt N Minuten
      addLog("Locklicht vor Öffnung (Zeitmodus)");
      preLightOpenDone = true;
    }
  }

  // ===== ZEITMODUS: LOCKLICHT VOR SCHLIESSEN =====
  if (automatikErlaubt && closeMode == "time" && doorPhase == PHASE_OPEN && doorOpen && !preLightCloseDone) {

    int closeMin = timeToMinutes(closeTime);
    int diffMin = closeMin - nowMin;
    if (diffMin < 0) diffMin += 1440;

    if (diffMin > 0 && diffMin <= lampPreClose && !preLightCloseDone) {
      lightState = LIGHT_PRE_CLOSE;
      startLightForMinutes(lampPreClose);
      addLog("Locklicht vor Schließen (Zeitmodus)");
      preLightCloseDone = true;
    }
  }

  // ===== LUX-TREND BERECHNUNG =====
  const unsigned long nowMs = millis();
  if (luxValid && lastLuxTime > 0 && nowMs - lastLuxTime >= LUX_TREND_INTERVAL_MS) {
    float dtMin = (nowMs - lastLuxTime) / 60000.0f;
    float luxRate = (lux - lastLux) / dtMin;
    luxRateFiltered = LUX_RATE_ALPHA * luxRate + (1.0f - LUX_RATE_ALPHA) * luxRateFiltered;
    lastLux = lux;
    lastLuxTime = nowMs;
  }

  // ===== LICHTMODUS – ENTKOPPELT (Vor-Licht nur Komfort, Öffnen nur Schwelle) =====
  if (automatikErlaubt && luxValid && openMode == "light" && !doorOpen && !learningActive && !nightLock) {

    // Außerhalb Öffnungsfenster: Vor-Licht aus, Stabilitäten resetten
    if (!inOpenWindow) {
      if (lightState == LIGHT_PRE_OPEN) {
        lightState = LIGHT_OFF;
        addLog("ℹ️ Öffnungsfenster zu – Vor-Licht beendet");
      }
      lightAboveSince = 0;
      openInterruptionSince = 0;
    } else {
      // -------- 1) Vor-Licht per PROGNOSE (stabilisiert) --------

      bool forecastNow = false;

      // Prognose-Bedingung prüfen
      if (luxRateFiltered > MIN_POS_LUX_RATE && lux < openLightThreshold) {

        float minutesToThresh = (openLightThreshold - lux) / luxRateFiltered;

        if (minutesToThresh >= (float)lampPreOpen - OPEN_FORECAST_TOLERANCE_MIN && minutesToThresh <= (float)lampPreOpen + OPEN_FORECAST_TOLERANCE_MIN) {

          forecastNow = true;
        }
      }

      // Zustand hat sich geändert?
      if (forecastNow != preLightForecastCondition) {
        preLightForecastCondition = forecastNow;
        preLightForecastSince = millis();
      }

      // Nur reagieren wenn 60 Sekunden stabil
      if ((millis() - preLightForecastSince) >= PRELIGHT_MIN_STABLE_MS) {

        // ---- START ----
        if (preLightForecastCondition && !preLightForecastActive) {

          lightState = LIGHT_PRE_OPEN;
          startLightForMinutesReset(max(1, lampPreOpen));

          addLogWithLux("Locklicht vor Öffnung gestartet (Prognose stabil)", lux);

          preLightForecastActive = true;
        }

        // ---- STOP (angepasst: halten bis Öffnung / Timeout) ----
        if (!preLightForecastCondition && preLightForecastActive) {

          const bool safetyTimeout = (millis() - preLightStartedAt) >= PRELIGHT_MAX_HOLD_MS;
          const bool thresholdNotReached = lux < openLightThreshold;

          if (preLightHoldEnabled && thresholdNotReached && !safetyTimeout) {
            // Nicht abschalten – wir halten bis zur Öffnung.
            // Damit updateLightState() das Licht nicht auslaufen lässt,
            // schieben wir die Restlaufzeit unauffällig nach vorn (Keep-Alive).
            if (lightState == LIGHT_PRE_OPEN && (long)(lightStateUntil - millis()) < 20000L) {
              startLightForMinutesReset(1);  // alle ~20-30s um 1 Min verlängern
            }
            // Kein Log – bewusst still weiterlaufen
          } else {
            // Wirklich aus – z. B. weil Timeout oder Schwelle bereits erreicht
            lightState = LIGHT_OFF;
            addLog("Locklicht vor Öffnung beendet (Prognose verworfen/Timeout)");
            preLightForecastActive = false;
          }
        }
      }
    }

    // -------- 2) Öffnung: NUR durch Schwellen-Erfüllung --------
    const int openDropThreshold = openLightThreshold - OPEN_HYSTERESIS_LX;

    // Öffnen wenn Schwelle erreicht ODER Sonnenaufgang erkannt
if (lux >= openLightThreshold || (predictSunrise() && lux > 10)) {
      openInterruptionSince = 0;
      if (lightAboveSince == 0) lightAboveSince = millis();

      if (millis() - lightAboveSince >= LIGHT_OPEN_DELAY_MS) {
        if (motorState == MOTOR_STOPPED) {
          doorPhase = PHASE_OPENING;
          motorReason = "Lichtautomatik";
          startMotorOpen(openPosition);
          addLog("Öffnung gestartet (Schwellen-Erfüllung)");

          // Aufräumen
          preLightCloseDone = false;
          preLightOpenDone = false;
          lightAboveSince = 0;
          openInterruptionSince = 0;
          // Vor-Licht darf weiterlaufen; POST_OPEN wird später aktiv
        }
      }
    } else if (lux < openDropThreshold) {
      if (openInterruptionSince == 0) openInterruptionSince = millis();
      if (millis() - openInterruptionSince > OPEN_GLITCH_MS) {
        lightAboveSince = 0;
        openInterruptionSince = 0;
      }
    }
  }



  // ===== WS2812 NACHT-DIMMUNG =====
  if (dimmingActive && lightActive) {
    if (nowMs >= dimStartTime && nowMs <= dimEndTime) {
      float progress =
        float(nowMs - dimStartTime) / float(dimEndTime - dimStartTime);
      progress = constrain(progress, 0.0f, 1.0f);
      int brightness = int(255 * (1.0f - progress));
      brightness = constrain(brightness, 0, 255);

      stallLight.setBrightness(brightness);
      for (int i = 0; i < WS2812_COUNT; i++)
        stallLight.setPixelColor(i, stallLight.Color(255, 180, 60));
      stallLight.show();
    }
    if (nowMs > dimEndTime) dimmingActive = false;
  }

  // ===== STALLLICHT AUTO-AUS =====
  if (stallLightActive && nowMs >= stallLightOffTime)
    stallLightOff();

  // ===== ÖFFNEN =====
if (automatikErlaubt && !learningActive && !doorOpen) {
  // Zeitmodus öffnen ODER Fallback bei Light+kein Lux
  if ((openMode == "time" || (openMode == "light" && !lightAutomationAvailable))
      && nowMin == timeToMinutes(openTime)
      && lastTimeActionMin != nowMin) {
    if (motorState == MOTOR_STOPPED) {
      doorPhase = PHASE_OPENING;
      motorReason = (openMode == "light" && !lightAutomationAvailable) ? "Zeit-Fallback" : "Zeitautomatik";
      startMotorOpen(openPosition);
      lastTimeActionMin = nowMin;
      preLightCloseDone = false;
      preLightOpenDone  = false;
    }
  }
}

// ===== SCHLIESSEN (Zeitmodus) =====
if (automatikErlaubt && !learningActive && doorOpen) {
  // Zeitmodus schließen ODER Fallback bei Light+kein Lux
  if ((closeMode == "time" || (closeMode == "light" && !lightAutomationAvailable))
      && nowMin == timeToMinutes(closeTime)) {
    if (motorState == MOTOR_STOPPED) {
      doorPhase = PHASE_CLOSING;
      motorReason = (closeMode == "light" && !lightAutomationAvailable) ? "Zeit-Fallback" : "Zeitautomatik";
      startMotorClose(closePosition);
      addLog("Schließvorgang gestartet (" + motorReason + ")");
      preLightOpenDone = false;
    }
  }
}
  


  // ===== LICHTMODUS: SCHLIESSEN =====
  if (automatikErlaubt && luxValid && closeMode == "light" && doorOpen && !learningActive && inCloseWindow) {
    // ------------------------------------------------------------
    // (A) Vor-Licht Prognose (nur Komfort, nur über Schwelle)
    // ------------------------------------------------------------
    if (!manualLightActive && lux > closeLightThreshold && lux < closeLightThreshold + CLOSE_FORECAST_MAX_DISTANCE_LX && luxRateFiltered < -MIN_NEG_LUX_RATE) {
            float minutesToThresh = 9999.0f;

if (!isfinite(luxRateFiltered) || fabs(luxRateFiltered) < 0.001f) {
    scheduledCloseAt = 0;
}
else {
    minutesToThresh = (lux - closeLightThreshold) / (-luxRateFiltered);
}

        if (minutesToThresh <= 0.5f && !preLightCloseDone) {

    if (lightState != LIGHT_PRE_CLOSE) {
        lightState = LIGHT_PRE_CLOSE;
        startLightForMinutesReset(max(1, lampPreClose));

        addLogWithLux("Locklicht vor Schließen gestartet (Prognose/sofort)", lux);

        preCloseStartedAt = millis();
        closeBrightTrendSince = 0;

        preLightCloseDone = true;   // ⭐ wichtig
    }

    scheduledCloseAt = 0;
} 
else if (lux > closeLightThreshold + PRECLOSE_ABORT_MARGIN_LX)  {
  // wieder deutlich heller → Prognose zurücksetzen
  preLightCloseDone = false;
}
else {
    unsigned long delayMs = (unsigned long)(minutesToThresh * 60000.0f);
    scheduledCloseAt = millis() + delayMs;
}
    }  
    // ------------------------------------------------------------
    // (B) Geplantes Vor-Licht auslösen
    // ------------------------------------------------------------
    if (scheduledCloseAt != 0 && millis() >= scheduledCloseAt) {
      if (!manualLightActive) {
        lightState = LIGHT_PRE_CLOSE;
        startLightForMinutesReset(max(1, lampPreClose));
        addLogWithLux("Locklicht vor Schließen gestartet (Prognose/Termin)", lux);
        preCloseStartedAt = millis();
        closeBrightTrendSince = 0;
      }
      scheduledCloseAt = 0;
    }

    // Keep-Alive: Vor-Schließen-Licht bis zum Schließen halten
    if (preCloseHoldEnabled && lightState == LIGHT_PRE_CLOSE) {
      const bool safetyTimeout = (millis() - preCloseStartedAt) >= PRECLOSE_MAX_HOLD_MS;
      if (!safetyTimeout && (long)(lightStateUntil - millis()) < 20000L) {
        startLightForMinutesReset(1);  // alle ~20–30 s frisch halten
      }

      // (C) Prognose nur bei deutlich & stabil heller abbrechen – sonst halten
      if (!inCloseWindow) {
        scheduledCloseAt = 0;
        if (lightState == LIGHT_PRE_CLOSE) {
          lightState = LIGHT_OFF;
          addLog("Locklicht vor Schließen beendet (Fenster)");
          // lastPreCloseAbortAt = millis(); // optional: Restart-Cooldown
        }
      } else {
        const bool abortCandidate =
          (luxRateFiltered > PRECLOSE_ABORT_POS_RATE) && (lux >= closeLightThreshold + PRECLOSE_ABORT_MARGIN_LX);

        if (abortCandidate) {
          if (closeBrightTrendSince == 0) closeBrightTrendSince = millis();

          if (millis() - closeBrightTrendSince >= PRECLOSE_ABORT_STABLE_MS) {
            const bool safetyTimeout = (millis() - preCloseStartedAt) >= PRECLOSE_MAX_HOLD_MS;

            if (preCloseHoldEnabled && !safetyTimeout && lightState == LIGHT_PRE_CLOSE) {
              // Latch aktiv → NICHT abbrechen; weiterlaufen lassen
            } else {
              scheduledCloseAt = 0;
              if (lightState == LIGHT_PRE_CLOSE) {
                lightState = LIGHT_OFF;
                addLog("Locklicht vor Schließen abgebrochen (stabil deutlich heller)");
                // lastPreCloseAbortAt = millis(); // optional
              }
            }
          }
        } else {
          closeBrightTrendSince = 0;  // kein stabiler deutlicher Aufhell-Trend
        }
      }
    }
    // ------------------------------------------------------------
    // (D) ECHTES SCHLIESSEN bei Schwellen-Erfüllung
    // ------------------------------------------------------------
    const int closeRiseThreshold = closeLightThreshold + CLOSE_HYSTERESIS_LX;

    if (lux <= closeLightThreshold) {
      closeInterruptionSince = 0;

      if (lightBelowSince == 0)
        lightBelowSince = millis();

      if (millis() - lightBelowSince >= LIGHT_DELAY_MS) {
        if (motorState == MOTOR_STOPPED) {
          doorPhase = PHASE_CLOSING;
          motorReason = "Lichtautomatik";
          startMotorClose(closePosition);

          addLog("Schließvorgang gestartet (Schwellen-Erfüllung)");

          lightBelowSince = 0;
          closeInterruptionSince = 0;
          scheduledCloseAt = 0;
        }
      }
    } else if (lux > closeRiseThreshold) {
      if (closeInterruptionSince == 0)
        closeInterruptionSince = millis();

      if (millis() - closeInterruptionSince > CLOSE_GLITCH_MS) {
        lightBelowSince = 0;
        closeInterruptionSince = 0;
      }
    }
  }

  // ===== EINMAL am Ende: Licht anwenden =====
  updateLightState();
}
/* ===================== STATUS JSON ===================== */

void handleStatus() {
  updateSystemHealth();

  DateTime nowDT = rtcOk ? rtc.now() : DateTime(2000, 1, 1, 0, 0, 0);
  String dateStr = rtcOk
      ? (String(nowDT.day()) + "." + String(nowDT.month()) + "." + String(nowDT.year()))
      : "-";

  String next = "-";
  if (!doorOpen) {
    next = (openMode == "time")
         ? "Öffnet um " + openTime
         : "Öffnet automatisch bei zunehmendem Licht (≈ " + String(openLightThreshold) + " lx)";
  } else {
    next = (closeMode == "time")
         ? "Schließt um " + closeTime
         : "Schließt automatisch bei abnehmendem Licht (≈ " + String(closeLightThreshold) + " lx)";
  }

  String automatik;
  if (openMode == "light" && closeMode == "light")      automatik = "🌞 Lichtautomatik aktiv";
  else if (openMode == "time" && closeMode == "time")   automatik = "⏰ Zeitautomatik aktiv";
  else                                                  automatik = "🔁 Mischbetrieb";

  StaticJsonDocument<768> doc;
  doc["time"]       = rtcOk ? nowDT.timestamp(DateTime::TIMESTAMP_TIME) : "--:--:--";
  doc["date"]       = dateStr;
  doc["door"]       = doorOpen ? "Offen" : "Geschlossen";
  doc["moving"]     = (motorState != MOTOR_STOPPED) ? "1" : "0";
  doc["doorBool"]   = doorOpen ? "1" : "0";
  doc["automatik"]  = automatik;
  doc["next"]       = next;
  doc["light"]      = (isfinite(lux) && lux >= 0.0f) ? String(lux, 1) + " lx" : "n/a";
  doc["lightFallback"] = (!lightAutomationAvailable && (openMode == "light" || closeMode == "light")) ? "1" : "0";
  doc["lightState"] = lightActive ? "An" : "Aus";
  doc["stallLight"] = stallLightActive ? "An" : "Aus";
  doc["manualLight"]= manualLightActive ? "1" : "0";
  doc["learning"]   = learningActive ? "ja" : "nein";
  doc["fw"]         = FW_VERSION;
  doc["sysError"]   = systemError() ? "1" : "0";

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

/* ===================== WEB UI (handleRoot) ===================== */
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="de" data-theme="%THEME%">
<head>

<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="default">
<link rel="apple-touch-icon" href="/icon-192.png">

<meta name="theme-color" content="#4CAF50">

<script>
let inflight = false; // Guard gegen parallele /status-Requests
let last = { date:"", time:"", door:"", automatik:"", next:"", light:"", lightState:"" };


// === Debounce-Helfer: Button kurz deaktivieren ===
async function debounceAction(btn, fn){
  if (!btn) return;
  btn.disabled = true;
  try { await fn(); }
  finally { setTimeout(() => btn.disabled = false, 250); }
}

// === Toggles mit Debounce & no-store ===
async function toggleLockLight(){
  const b = document.getElementById('lockLightBtn');
  await debounceAction(b, async () => {
    await fetch('/light', { cache:'no-store' });
    await update();
  });
}
async function toggleStallLight(){
  const b = document.getElementById('stallLightBtn');
  await debounceAction(b, async () => {
    await fetch('/stalllight', { cache:'no-store' });
    await update();
  });
}
async function toggleDoor(){
  const b = document.getElementById('doorBtn');
  await debounceAction(b, async () => {
    await fetch('/door', { cache:'no-store' });
    await update();
  });
}
function setDoorButton(label, cls) {
  const b = document.getElementById('doorBtn');
  if (!b) return;
  b.textContent = label;
  b.classList.remove('btn-open','btn-close','btn-stop');
  b.classList.add(cls);
}

// Sichtbarkeitswechsel: sofort refreshe
document.addEventListener('visibilitychange', () => {
  if (document.visibilityState === 'visible') update();
});


// Erste Aktualisierung und Polling erst starten, wenn DOM komplett ist
document.addEventListener('DOMContentLoaded', () => {
  update();                 // sofort erste Daten holen
  setInterval(update, 3000); // danach alle 3s
});

</script>

<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<title>Hühnerklappe</title>

<style>

/* ====== ROOT DESIGN SYSTEM ====== */
:root {
  --bg: #f4f6f9;
  --card: #ffffff;
  --text: #1f2937;
  --muted: #6b7280;
  --green: #22c55e;
  --red: #ef4444;
  --orange: #f59e0b;
  --accent: #16a34a;
  --radius: 18px;
}

/* AUTO */
@media (prefers-color-scheme: dark) {
  :root[data-theme="auto"] {
    --bg:#0f172a;
    --card:#1e293b;
    --text:#f1f5f9;
    --muted:#94a3b8;
  }

/* MANUELL DARK */
:root[data-theme="dark"] {
  --bg:#0f172a;
  --card:#1e293b;
  --text:#f1f5f9;
  --muted:#94a3b8;
}

/* MANUELL LIGHT */
:root[data-theme="light"] {
  --bg:#f4f6f9;
  --card:#ffffff;
  --text:#1f2937;
  --muted:#6b7280;
}

}

body {
  margin: 0;
  font-family: system-ui, -apple-system, BlinkMacSystemFont, sans-serif;
  background: var(--bg);
  color: var(--text);
  -webkit-font-smoothing: antialiased;
}

/* ====== HEADER ====== */
.header {
  padding: 28px 20px 20px;
  text-align: center;
  font-size: 20px;
  font-weight: 700;
}

.datetime {
  font-size: 15px;
  color: var(--muted);
  margin-top: 6px;
}

/* ====== CONTAINER ====== */
.container {
  max-width: 430px;
  margin: auto;
  padding: 10px 16px 110px;
}

/* ====== CARD ====== */
.card {
  background: var(--card);
  border-radius: var(--radius);
  padding: 18px;
  margin-bottom: 16px;
  box-shadow:
    0 6px 18px rgba(0,0,0,0.06),
    0 2px 6px rgba(0,0,0,0.04);
  transition: transform .15s ease;
}

.card:active {
  transform: scale(0.98);
}

.card-title {
  font-size: 16px;
  font-weight: 600;
  margin-bottom: 14px;
}

/* ====== STATUS BADGES ====== */
.badge {
  padding: 6px 16px;
  border-radius: 999px;
  font-weight: 600;
  font-size: 14px;
}

.open {
  background: rgba(34,197,94,0.15);
  color: var(--green);
}

.closed {
  background: rgba(239,68,68,0.15);
  color: var(--red);
}

/* ====== STATUS ROW ====== */
.status-row {
  display: flex;
  justify-content: space-between;
  margin-bottom: 10px;
  font-size: 15px;
}

.status-row .label {
  color: var(--muted);
}

/* ====== LUX BAR ====== */
.luxbar {
  height: 8px;
  background: rgba(0,0,0,0.08);
  border-radius: 999px;
  margin-top: 8px;
  overflow: hidden;
}

.luxfill {
  height: 100%;
  width: 0%;
  background: linear-gradient(90deg,#facc15,#f97316);
  transition: width .5s ease;
}

/* ====== BUTTONS ====== */
button {
  border: none;
  border-radius: 14px;
  padding: 14px;
  font-size: 17px;
  font-weight: 600;
  cursor: pointer;
  transition: all .15s ease;
}

.btn-open {
  width: 100%;
  background: var(--green);
  color: white;
}

.btn-close {
  width: 100%;
  background: var(--red);
  color: white;
}

.btn-stop {
  width: 100%;
  background: var(--orange);
  color: white;
}

/* ====== LIGHT BUTTONS ====== */
.light-row {
  display: flex;
  justify-content: space-between;
  gap: 12px;
}

.light-btn {
  flex: 1;
  height: 56px;
  border-radius: 16px;
  font-size: 20px;
  background: var(--card);
  box-shadow:
    inset 0 1px 3px rgba(0,0,0,0.05),
    0 4px 12px rgba(0,0,0,0.08);
}

.light-btn.on {
  background: var(--green);
  color: white;
  box-shadow: 0 0 10px rgba(34,197,94,0.6);
}
  
/* ====== NEXT ACTION ====== */
.next-section {
  font-size: 14px;
  color: var(--muted);
  line-height: 1.4;
}

/* ====== SYSTEM STATUS ICON ====== */
.sys-ok, .sys-error {
  width: 30px;
  height: 30px;
  border-radius: 50%;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  font-size: 16px;
}

.sys-ok {
  background: rgba(34,197,94,0.15);
  color: var(--green);
}

.sys-error {
  background: rgba(239,68,68,0.15);
  color: var(--red);
}

/* ====== FOOTER NAV ====== */
nav {
  position: fixed;
  bottom: 18px;
  left: 50%;
  transform: translateX(-50%);
  width: 100%;
  max-width: 430px;
  background: var(--card);
  border-radius: 22px;
  box-shadow: 0 10px 25px rgba(0,0,0,0.15);
  display: flex;
  padding: 6px 0;
}

nav a {
  flex: 1;
  text-align: center;
  text-decoration: none;
  color: var(--muted);
  font-size: 13px;
  font-weight: 600;
}

nav a:active {
  transform: scale(0.92);
}
.card-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 10px;
}

.card-title {
  margin: 0;
  font-size: 16px;
  font-weight: 600;
}

.sys-link {
  text-decoration: none;
}

.sys-ok, .sys-error {
  width: 28px;
  height: 28px;
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 14px;
}

/* AUTO */
@media (prefers-color-scheme: dark) {
  :root[data-theme="auto"] {
    --bg:#0f172a;
    --card:#1e293b;
    --text:#f1f5f9;
    --muted:#94a3b8;
  }
}

/* MANUELL DARK */
:root[data-theme="dark"] {
  --bg:#0f172a;
  --card:#1e293b;
  --text:#f1f5f9;
  --muted:#94a3b8;
}

/* MANUELL LIGHT */
:root[data-theme="light"] {
  --bg:#f4f6f9;
  --card:#ffffff;
  --text:#1f2937;
  --muted:#6b7280;
}
</style>
</head>

<body>

<div class="header">
  <h3>🐔 Hühnerklappe</h3>
  <div class="datetime">
    <span id="date">---</span> · <span id="time">--:--</span>
  </div>
  <hr class="header-line">
</div>

<div class="container">

  <div class="card">

  <div class="card-header">
    <h3 class="card-title">Türstatus</h3>
  <a href="/systemtest" id="sysStatusLink" class="sys-link">
    <span id="sysStatus" class="sys-ok">✔</span>
  </a>  </div>

  <hr class="card-line">

  <div class="status-row">
    <span class="label">Stallklappe:</span>
    <span id="door" class="badge closed">---</span>
  </div>

  <div class="status-row">
    <span class="label">Locklicht:</span>
    <span id="lightState" class="badge closed">---</span>
  </div>

  <div class="status-row">
    <span class="label">Stalllicht:</span>
    <span id="stallLight" class="badge closed">---</span>
  </div>

  <div class="status-row">
    <span class="label">Betriebsart:</span>
    <span id="automatik">---</span>
  </div>

  <div class="status-row">
    <span class="label">Helligkeit:</span>
    <span id="light">---</span>
  </div>
  
  <div class="luxbar">
  <div id="luxFill" class="luxfill"></div>
  </div>
  
  <hr class="card-line">

  <div class="next-section">
    <div class="next-title">Nächste Aktion:</div>
    <div id="next" class="next-value-full">---</div>
  </div>

</div>


  <div class="card">
    <!-- 🔆 LICHT-BUTTONS -->
    <div class="light-row">
      <!-- Locklicht -->
      <button type="button" id="lockLightBtn"
  class="light-btn small off"
  onclick="toggleLockLight()"
  title="Locklicht">🔆</button>

<button type="button" id="stallLightBtn"
  class="light-btn led off"
  onclick="toggleStallLight()"
  title="Stalllicht">
  <!-- Dein LED-SVG hier oder 💡 -->
  💡
</button>
    </div>

    <hr class="card-line">

    <!-- 🚪 TÜR -->
    <button type="button" id="doorBtn" class="btn-open" onclick="toggleDoor()">Öffnen</button>
  </div>

</div>

<nav>
  <a href="/">🏠<br>Start</a>
  <a href="/settings">⚙️<br>Einstellungen</a>
  <a href="/advanced">🔧<br>Erweitert</a>
  </nav>


<script>


async function update(){
  if (inflight) return;          // ✅ Guard VOR dem fetch
  inflight = true;
  try{
    const r = await fetch('/status', { cache:'no-store' });
    const d = await r.json();

    if (d.date !== last.date) { document.getElementById('date').innerText = d.date; last.date = d.date; }
    if (d.time !== last.time) { document.getElementById('time').innerText = d.time; last.time = d.time; }

    if (d.door !== last.door) {
      const el = document.getElementById('door');
      el.innerText = d.door;
      el.className = "badge " + (d.door === "Offen" ? "open" : "closed");
      last.door = d.door;
    }

    const moving = (d.moving === "1");
    const isOpen = (d.door === "Offen");
    if (moving)       setDoorButton("Stopp","btn-stop");
    else if (isOpen)  setDoorButton("Schließen","btn-close");
    else              setDoorButton("Öffnen","btn-open");

    if (d.automatik !== last.automatik) { document.getElementById('automatik').innerText = d.automatik; last.automatik = d.automatik; }
    if (d.next !== last.next)           { document.getElementById('next').innerText = d.next;           last.next = d.next; }
    if (d.light !== last.light)         { document.getElementById('light').innerText = d.light;         last.light = d.light; }

// ===== Systemstatus IMMER prüfen =====
const sys = document.getElementById("sysStatus");

if (d.sysError === "1") {
  sys.innerHTML = "✖";              // rotes X
  sys.className = "sys-error";
} else {
  sys.innerHTML = "✔";              // grüner Haken
  sys.className = "sys-ok";
}


// ===== Locklicht nur bei Änderung =====
if (d.lightState !== last.lightState) {
  const ls = document.getElementById('lightState');
  ls.innerText = d.lightState;
  ls.className = (d.lightState === "An") ? "badge open" : "badge closed";
  last.lightState = d.lightState;
}


    const stall = document.getElementById("stallLight");
    if (stall && d.stallLight) {
      stall.innerText = d.stallLight;
      stall.className = "badge " + (d.stallLight === "An" ? "open" : "closed");
    }
      // ===== Licht Buttons hervorheben =====
const lockBtn = document.getElementById("lockLightBtn");
const stallBtn = document.getElementById("stallLightBtn");

if (lockBtn) {
  lockBtn.classList.toggle("on", d.lightState === "An");
}

if (stallBtn) {
  stallBtn.classList.toggle("on", d.stallLight === "An");
}
  } catch(e){
    // optional: console.log(e);
  } finally {
    inflight = false;            // ✅ Immer freigeben, auch bei Fehlern
  }
}
</script>


</body>
</html>
)rawliteral";

  html.replace("%THEME%", uiTheme);
  server.send(200, "text/html; charset=UTF-8", html);
}
/* ===================== SETTINGS (handleSettings) ===================== */
void handleSettings() {

  String html = renderThemeHead("Einstellungen");

  html += R"rawliteral(

<div class="header">
  <h3>⚙️ Einstellungen</h3>
</div>

<div class="container">

  <div class="card">

    <div class="card-title">Betriebsmodus</div>

    <div class="tabs">
      <button class="tab-btn active" onclick="showTab('open')">Öffnen</button>
      <button class="tab-btn" onclick="showTab('close')">Schließen</button>
    </div>
<div class="switch-row">
  <span>Endschalter verwenden</span>
  <label class="switch">
    <input type="checkbox" id="limitSwitches" %LIMIT_SWITCHES%>
    <span class="slider"></span>
  </label>
</div>
<br>
  </div>

  <!-- ================= OPEN ================= -->
  <div class="card" id="openCard">

    <div class="card-title">Öffnungs-Einstellungen</div>

    <form id="openTab">

      <label>Modus</label>
      <select name="openMode" id="openMode" onchange="toggleOpen()">
        <option value="time">Zeit</option>
        <option value="light">Helligkeit</option>
      </select>

      <div id="openTimeField">
        <label>Öffnungszeit</label>
        <input type="time" name="openTime">
      </div>

      <div id="openLightField" style="display:none;">
        <label>Licht-Schwelle (Lux)</label>
        <input type="number" name="openLightThreshold" class="light-input">
      </div>

      <label>Licht vorher (Min)</label>
      <input type="number" class="light-input" name="lampPreOpen">

      <label>Licht nachher (Min)</label>
      <input type="number" class="light-input" name="lampPostOpen">

      <button type="submit" class="btn-open">Speichern</button>

    </form>
  </div>

  <!-- ================= CLOSE ================= -->
  <div class="card" id="closeCard" style="display:none;">

    <div class="card-title">Schließ-Einstellungen</div>

    <form id="closeTab">

      <label>Modus</label>
      <select name="closeMode" id="closeMode" onchange="toggleClose()">
        <option value="time">Zeit</option>
        <option value="light">Helligkeit</option>
      </select>

      <div id="closeTimeField">
        <label>Schließzeit</label>
        <input type="time" name="closeTime">
      </div>

      <div id="closeLightField" style="display:none;">
        <label>Licht-Schwelle (Lux)</label>
        <input type="number" class="light-input" name="closeLightThreshold">
      </div>

      <label>Licht vorher (Min)</label>
      <input type="number" class="light-input" name="lampPreClose">

      <label>Licht nachher (Min)</label>
      <input type="number" class="light-input"name="lampPostClose">

      <button type="submit" class="btn-close">Speichern</button>

    </form>
  </div>

  <div id="saveMsg" class="card" style="display:none;text-align:center;font-weight:600;"></div>

</div>

<style>

/* ===== Tabs Styling ===== */
.tabs {
  display:flex;
  gap:10px;
  margin-top:10px;
}

.tab-btn {
  flex:1;
  padding:8px;
  border-radius:12px;
  border:none;
  font-weight:600;
  font-size:14px;
  background:var(--bg);
  color:var(--text);
}

.tab-btn.active {
  background:var(--green);
  color:white;
}

/* ===== Inputs ===== */
label {
  display:block;
  font-weight:600;
  margin-top:14px;
  margin-bottom:6px;
  font-size:14px;
  color:var(--muted);
}

input, select {
  width:100%;
  padding:8px 10px;
  border-radius:10px;
  border:1px solid #e5e7eb;
  font-size:14px;
  background:var(--card);
  color:var(--text);
  height:38px;
  box-sizing:border-box;
}
.light-input {
  }
/* ===== Modern Toggle Switch ===== */

.switch-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin: 14px 0;
}

.switch {
  position: relative;
  display: inline-block;
  width: 52px;
  height: 28px;
}

.switch input {
  opacity: 0;
  width: 0;
  height: 0;
}

.slider {
  position: absolute;
  cursor: pointer;
  inset: 0;
  background-color: #2c3e50;
  border-radius: 34px;
  transition: 0.25s;
}

.slider:before {
  position: absolute;
  content: "";
  height: 22px;
  width: 22px;
  left: 3px;
  bottom: 3px;
  background-color: white;
  border-radius: 50%;
  transition: 0.25s;
  box-shadow: 0 2px 6px rgba(0,0,0,0.4);
}

input:checked + .slider {
  background-color: #27ae60;
}

input:checked + .slider:before {
  transform: translateX(24px);
}
</style>

<script>

function showTab(which){
  document.getElementById('openCard').style.display  = (which==='open')?'block':'none';
  document.getElementById('closeCard').style.display = (which==='close')?'block':'none';

  const tabs=document.querySelectorAll('.tab-btn');
  tabs[0].classList.toggle('active', which==='open');
  tabs[1].classList.toggle('active', which==='close');
}

function toggleOpen(){
  let m=document.getElementById('openMode').value;
  document.getElementById('openTimeField').style.display  = (m==='time')?'block':'none';
  document.getElementById('openLightField').style.display = (m==='light')?'block':'none';
}

function toggleClose(){
  let m=document.getElementById('closeMode').value;
  document.getElementById('closeTimeField').style.display  = (m==='time')?'block':'none';
  document.getElementById('closeLightField').style.display = (m==='light')?'block':'none';
}

/* ===== Save OPEN ===== */
document.getElementById('openTab').onsubmit = e=>{
  e.preventDefault();
  fetch('/save-open',{method:'POST',body:new FormData(e.target)})
    .then(()=>showMsg("Öffnen gespeichert",true));
};

/* ===== Save CLOSE ===== */
document.getElementById('closeTab').onsubmit = e=>{
  e.preventDefault();
  fetch('/save-close',{method:'POST',body:new FormData(e.target)})
    .then(()=>showMsg("Schließen gespeichert",true));
};

function showMsg(text,ok){
  const msg=document.getElementById("saveMsg");
  msg.innerText=text;
  msg.style.background= ok ? "rgba(34,197,94,0.15)" : "rgba(239,68,68,0.15)";
  msg.style.color= ok ? "var(--green)" : "var(--red)";
  msg.style.display="block";
  setTimeout(()=>msg.style.display="none",1500);
}

/* ===== Werte vom ESP ===== */
document.getElementById('openMode').value = "%OPEN_MODE%";
document.querySelector('[name="openTime"]').value = "%OPEN_TIME%";
document.querySelector('[name="openLightThreshold"]').value = "%OPEN_LIGHT%";
document.querySelector('[name="lampPreOpen"]').value = "%LAMP_PRE_OPEN%";
document.querySelector('[name="lampPostOpen"]').value = "%LAMP_POST_OPEN%";

document.getElementById('closeMode').value = "%CLOSE_MODE%";
document.querySelector('[name="closeTime"]').value = "%CLOSE_TIME%";
document.querySelector('[name="closeLightThreshold"]').value = "%CLOSE_LIGHT%";
document.querySelector('[name="lampPreClose"]').value = "%LAMP_PRE_CLOSE%";
document.querySelector('[name="lampPostClose"]').value = "%LAMP_POST_CLOSE%";

showTab('open');
toggleOpen();
toggleClose();
// ===== Endschalter speichern =====
document.getElementById("limitSwitches").addEventListener("change", function() {

  const enabled = this.checked ? 1 : 0;

  fetch("/set-limit-switches", {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded" },
    body: "enabled=" + enabled
  })
  .then(() => showMsg("Endschalter gespeichert", true));

});
</script>

)rawliteral";

  html += renderFooter();

  html.replace("%OPEN_MODE%", openMode);
  html.replace("%OPEN_TIME%", openTime);
  html.replace("%OPEN_LIGHT%", String(openLightThreshold));
  html.replace("%LAMP_PRE_OPEN%", String(lampPreOpen));
  html.replace("%LAMP_POST_OPEN%", String(lampPostOpen));
  html.replace("%CLOSE_MODE%", closeMode);
  html.replace("%CLOSE_TIME%", closeTime);
  html.replace("%CLOSE_LIGHT%", String(closeLightThreshold));
  html.replace("%LAMP_PRE_CLOSE%", String(lampPreClose));
  html.replace("%LAMP_POST_CLOSE%", String(lampPostClose));
  html.replace("%LIMIT_SWITCHES%", useLimitSwitches ? "checked" : "");

  server.send(200, "text/html; charset=UTF-8", html);
}
/* ===================== Erweiterte SETTINGS (handleSettings) ===================== */
void handleAdvanced() {

  String html = renderThemeHead("Erweitert");

  html += R"rawliteral(

<div class="header">
  <h3>🔧 Erweitert</h3>
</div>

<div class="container">

  <!-- ===== SYSTEM INFO ===== -->
  <div class="card">
    <div class="card-title">System</div>

    <div class="status-row">
      <span class="label">Firmware</span>
      <span>%FW_VERSION%</span>
    </div>

    <div class="status-row">
      <span class="label">WLAN Signal</span>
      <span>%RSSI% dBm</span>
    </div>

    <div class="status-row">
      <span class="label">Freier Speicher</span>
      <span>%FREE_HEAP% KB</span>
    </div>
  </div>

  <!-- ===== TOOLS ===== -->
  <div class="card">
    <div class="card-title">Werkzeuge</div>

    <button onclick="location.href='/systemtest'" class="btn-open">
      🧪 Systemtest
    </button>

    <button onclick="location.href='/mqtt'" class="btn-open">
      📡 MQTT Einstellungen
    </button>

    <button onclick="location.href='/calibration'" class="btn-open">
      🎯 Kalibrierung
    </button>

    <button onclick="location.href='/log'" class="btn-open">
      📜 Logbuch
    </button>

    <button onclick="location.href='/fw'" class="btn-open">
      ⬆️ Firmware Update
    </button>

     <button onclick="toggleTheme()" class="btn-open">
      🌙 Template (DarkMode)
    </button>

  </div>


  <!-- ===== DANGER ZONE ===== -->
  <div class="card danger-zone">
    <div class="card-title" style="color:var(--red);">
      ⚠️ System
    </div>

    <button class="btn-close" onclick="rebootESP()">
      🔄 ESP Neustart
    </button>

    <button class="btn-close" onclick="factoryReset()">
      🧨 Werkseinstellungen
    </button>
  </div>

</div>

<style>

.status-row {
  display:flex;
  justify-content:space-between;
  margin-bottom:12px;
  font-size:15px;
}

.label {
  color:var(--muted);
}

.danger-zone {
  border:1px solid rgba(239,68,68,0.3);
}

.btn-open {
  margin-top:10px;
  background:var(--green);
}

.btn-close {
  margin-top:10px;
  background:var(--red);
}

</style>

<script>

function rebootESP(){
  if(!confirm("ESP wirklich neu starten?")) return;
  fetch("/reset",{method:"POST"});
  setTimeout(()=>location.href="/",4000);
}

function factoryReset(){
  if(!confirm("Alle Einstellungen löschen?")) return;
  fetch("/factory-reset",{method:"POST"});
  setTimeout(()=>location.href="/",4000);
}

</script>

)rawliteral";

  html += renderFooter();

  html.replace("%FW_VERSION%", FW_VERSION);
  html.replace("%RSSI%", String(WiFi.RSSI()));
  html.replace("%FREE_HEAP%", String(ESP.getFreeHeap() / 1024));

  server.send(200, "text/html; charset=UTF-8", html);
}
/* ===================== SAVE HANDLER ===================== */
void handleSaveOpen() {
  strncpy(settings.openMode, server.arg("openMode").c_str(), 6);
  settings.openMode[5] = '\0';
  strncpy(settings.openTime, server.arg("openTime").c_str(), 6);
  settings.openTime[5] = '\0';

  settings.openLightThreshold = server.arg("openLightThreshold").toInt();
  settings.lampPreOpen = server.arg("lampPreOpen").toInt();
  settings.lampPostOpen = server.arg("lampPostOpen").toInt();

  if (settings.openLightThreshold <= settings.closeLightThreshold) {
    server.send(400, "text/plain",
                "Fehler: Öffnen-Lux muss größer sein als Schließen-Lux!");
    return;
  }

  saveSettings();

  openMode = settings.openMode;
  openTime = settings.openTime;
  openLightThreshold = settings.openLightThreshold;
  lampPreOpen = settings.lampPreOpen;
  lampPostOpen = settings.lampPostOpen;

  server.send(200, "text/plain", "OK");
}


void handleSaveClose() {
  strncpy(settings.closeMode, server.arg("closeMode").c_str(), 6);
  settings.closeMode[5] = '\0';
  strncpy(settings.closeTime, server.arg("closeTime").c_str(), 6);
  settings.closeTime[5] = '\0';

  settings.closeLightThreshold = server.arg("closeLightThreshold").toInt();
  settings.lampPreClose = server.arg("lampPreClose").toInt();
  settings.lampPostClose = server.arg("lampPostClose").toInt();

  // ✅ NEU: Serverseitige Validierung – Schließen-Lux muss kleiner sein als Öffnen-Lux
  // Nutze den aktuell gültigen Öffnen-Lux (aus RAM, bereits konsistent mit EEPROM nach loadSettings)
  if (settings.closeLightThreshold >= openLightThreshold) {
    server.send(400, "text/plain",
                "Fehler: Schließen-Lux muss kleiner sein als Öffnen-Lux!");
    return;
  }

  saveSettings();

  closeMode = settings.closeMode;
  closeTime = settings.closeTime;
  closeLightThreshold = settings.closeLightThreshold;
  lampPreClose = settings.lampPreClose;
  lampPostClose = settings.lampPostClose;

  server.send(200, "text/plain", "OK");
}

/* ===================== KALIBRIERUNG / LEARN PAGES ===================== */

void handleCalibration() {

  String html = renderThemeHead("Kalibrierung");

  html += R"rawliteral(

<div class="header">
  <h3>🎯 Kalibrierung</h3>
</div>

<div class="container">

  <!-- ===== EINLERNEN ===== -->
  <div class="card">
    <div class="card-title">🧰 Einlernen</div>

    <p style="font-size:14px;color:var(--muted);">
      Fahre die Klappe in die gewünschte Endposition und starte den Einlernmodus.
    </p>

    <button class="btn-open" onclick="startLearn()">
      Einlernmodus starten
    </button>

    <div id="learnStatus" style="margin-top:12px;font-size:14px;"></div>
  </div>

  <!-- ===== MANUELLE STEUERUNG ===== -->
  <div class="card">
    <div class="card-title">🎚 Manuelle Steuerung</div>

    <div style="display:flex;gap:10px;">
      <button class="btn-open" style="flex:1;" onclick="motorUp()">⬆ Hoch</button>
      <button style="flex:1;background:var(--orange);" onclick="motorStop()">⏹ Stop</button>
      <button class="btn-close" style="flex:1;" onclick="motorDown()">⬇ Runter</button>
    </div>

    <div id="motorStatus" style="margin-top:12px;font-size:14px;"></div>
  </div>

  <!-- ===== POSITIONEN ===== -->
  <div class="card">
    <div class="card-title">📊 Endpositionen</div>

    <div style="display:flex;gap:16px;margin-top:10px;">
      <div style="flex:1;background:var(--bg);padding:14px;border-radius:12px;text-align:center;">
        <div style="font-size:13px;color:var(--muted);">Offen</div>
        <div id="posOpen" style="font-weight:bold;font-size:18px;">--</div>
      </div>

      <div style="flex:1;background:var(--bg);padding:14px;border-radius:12px;text-align:center;">
        <div style="font-size:13px;color:var(--muted);">Geschlossen</div>
        <div id="posClose" style="font-weight:bold;font-size:18px;">--</div>
      </div>
    </div>
  </div>

</div>

<script>

/* ===== MOTOR ===== */

function motorUp(){
  fetch("/motor/up");
  document.getElementById("motorStatus").innerHTML="⬆ Motor läuft hoch...";
}

function motorDown(){
  fetch("/motor/down");
  document.getElementById("motorStatus").innerHTML="⬇ Motor läuft runter...";
}

function motorStop(){
  fetch("/motor/stop");
  document.getElementById("motorStatus").innerHTML="⏹ Motor gestoppt.";
}

/* ===== EINLERNEN ===== */

function startLearn(){
  document.getElementById("learnStatus").innerHTML="⏳ Einlernmodus wird gestartet...";
  fetch("/learn-start",{method:"POST"})
    .then(r=>{
      if(r.ok){
        document.getElementById("learnStatus").innerHTML="✅ Einlernmodus aktiv.";
      }else{
        document.getElementById("learnStatus").innerHTML="❌ Fehler beim Start.";
      }
    });
}

/* ===== POSITION LIVE UPDATE ===== */

function updatePositions(){
  fetch("/calib-status")
    .then(r=>r.json())
    .then(data=>{
      document.getElementById("posOpen").innerHTML=data.open;
      document.getElementById("posClose").innerHTML=data.close;
    })
    .catch(()=>{});
}

setInterval(updatePositions,2000);
updatePositions();

</script>

)rawliteral";

  html += renderFooter();
  server.send(200, "text/html; charset=UTF-8", html);
}

/* ===================== KALIBRIERUNG / LEARN PAGES ===================== */
void handleLearn() {

  if (learningActive) {
    server.send(200, "text/plain", "Learning already active");
    return;
  }

  if (motorState != MOTOR_STOPPED) {
    server.send(409, "text/plain", "Motor running");
    return;
  }

  learningActive = true;
  learningOpenDone = false;

  // Erste Phase: Öffnen lernen
  learnStartTime = millis();
  motorReason = "Einlernen";

  startMotorOpen(20000);  // max 20s Sicherheitslauf

  addLog("Einlernen gestartet – fahre Richtung OPEN");

  server.send(200, "text/plain", "Learning started");
}



void handleLearnPage() {
  server.send(200, "text/html; charset=UTF-8", R"rawliteral(
<!DOCTYPE html>
<html lang="de"><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Einlernen</title>
<style>
body {
  font-family: Arial, sans-serif;
  background: #f2f4f7;
  margin: 0;
  padding: 20px;
  padding-bottom:70px;
}
.card {
  background: white;
  padding: 20px;
  border-radius: 12px;
  box-shadow: 0 2px 6px rgba(0,0,0,0.1);
  text-align: center;
}
button {
  width: 100%;
  padding: 14px;
  font-size: 18px;
  border: none;
  border-radius: 8px;
  background: #4CAF50;
  color: white;
}
a {
  display: block;
  margin-top: 16px;
  text-decoration: none;
  color: #333;
  font-weight: bold;
}
</style></head>
<body>
<div class="card">
  <h2>Einlernen starten?</h2>
  <p>Die Klappe wird geoeffnet.<br>
     Druecke den Taster, um die Endlagen zu speichern.</p>

  <button onclick="startLearn()">Ja, Einlernen starten</button>

  <script>
    function startLearn() {
      fetch('/learn').then(() => {
        window.location.href = '/';
      });
    }
  </script>

  <a href="/">Abbrechen</a>
  <div class="card">
  <button class="btn-danger" onclick="location.href='/'">
    ⬅ Zurück zur Startseite
  </button>
</div>
</div>
<nav style="position:fixed; bottom:0; left:0; right:0;
background:white; border-top:1px solid #ddd;
display:flex; justify-content:space-around;
padding:8px 0;">

  <a href="/settings" style="text-decoration:none; color:#333;">
    ⚙ Einstellungen
  </a>

  <a href="/fw" style="text-decoration:none; color:#333;">
    ⬆ Firmware
  </a>

</nav>

</body></html>
)rawliteral");
}

/* ===================== UTILS ===================== */
String renderThemeHead(String title) {
  return String(R"rawliteral(
<!DOCTYPE html>
<html lang="de" data-theme=")rawliteral"
                + uiTheme + R"rawliteral(">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>)rawliteral")
         + title + R"rawliteral(</title>

<style>
:root {
  --bg:#f4f6f9;
  --card:#ffffff;
  --text:#1f2937;
  --muted:#6b7280;
  --green:#22c55e;
  --red:#ef4444;
  --orange:#f59e0b;
  --radius:18px;
}

/* AUTO */
@media (prefers-color-scheme: dark) {
  :root[data-theme="auto"] {
    --bg:#0f172a;
    --card:#1e293b;
    --text:#f1f5f9;
    --muted:#94a3b8;
  }
}

/* MANUELL DARK */
:root[data-theme="dark"] {
  --bg:#0f172a;
  --card:#1e293b;
  --text:#f1f5f9;
  --muted:#94a3b8;
}

/* MANUELL LIGHT */
:root[data-theme="light"] {
  --bg:#f4f6f9;
  --card:#ffffff;
  --text:#1f2937;
  --muted:#6b7280;
}
body{
  margin:0;
  font-family:system-ui,-apple-system;
  background:var(--bg);
  color:var(--text);
}
/* ===== HEADER ===== */
.header {
  text-align: center;
  padding: 26px 0 12px;
}

.header h1,
.header h2,
.header h3 {
  margin: 0;
  font-weight: 700;
}
.container{
  max-width:430px;
  margin:auto;
  padding:20px 16px 110px;
}

.card{
  background:var(--card);
  padding:18px;
  border-radius:var(--radius);
  margin-bottom:16px;
  box-shadow:0 6px 18px rgba(0,0,0,0.06);
}

button{
  width:100%;
  padding:14px;
  border:none;
  border-radius:14px;
  font-size:16px;
  font-weight:600;
  background:var(--green);
  color:white;
  margin-top:12px;
}

button.danger{
  background:var(--red);
}

nav{
  position:fixed;
  bottom:18px;
  left:50%;
  transform:translateX(-50%);
  width:100%;
  max-width:430px;
  background:var(--card);
  border-radius:22px;
  box-shadow:0 10px 25px rgba(0,0,0,0.15);
  display:flex;
  padding:6px 0;
}

nav a{
  flex:1;
  text-align:center;
  text-decoration:none;
  color:var(--muted);
  font-size:13px;
  font-weight:600;
}
</style>
<script>
function toggleTheme(){

  let current = document.documentElement.getAttribute("data-theme");

  let next;

  if(current === "auto") next = "dark";
  else if(current === "dark") next = "light";
  else next = "auto";

  fetch("/set-theme", {
    method:"POST",
    headers:{"Content-Type":"application/x-www-form-urlencoded"},
    body:"theme=" + next
  }).then(()=>{
    document.documentElement.setAttribute("data-theme", next);
  });
}
</script>
</head>
<body>
<div class="container">
)rawliteral";
}

String renderFooter() {
  return R"rawliteral(
</div>
<nav>
  <a href="/">🏠<br>Start</a>
  <a href="/settings">⚙️<br>Einstellungen</a>
  <a href="/advanced">🔧<br>Erweitert</a>
</nav>
</body>
</html>
)rawliteral";
}
int timeToMinutes(String t) {
  int sep = t.indexOf(':');
  if (sep == -1) return 0;
  int hour = t.substring(0, sep).toInt();
  int minute = t.substring(sep + 1).toInt();
  return hour * 60 + minute;
}
void handleSelftest() {

  String html = renderThemeHead("Systemtest");

  html += R"rawliteral(

<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Systemtest</title>
</head>
<body>

<div class="header">
  <h3>🧪 Systemtest</h3>
</div>

<div class="container">

  <!-- ===== SYSTEM ===== -->
  <div class="card">
    <div class="card-title">System</div>
    <div class="divider"></div>

    <div class="status-row">
      <span class="label">Freier Speicher</span>
      <span id="heapStatus">%FREE_HEAP% KB</span>
    </div>

    <div class="status-row">
      <span class="label">Firmware</span>
      <span>%FW_VERSION%</span>
    </div>
  </div>

  <!-- ===== SENSOREN ===== -->
  <div class="card">
    <div class="card-title">Sensoren</div>
    <div class="divider"></div>

    <div class="status-row">
      <span class="label">Lichtsensor</span>
      <!-- initial neutral; wird per JS überschrieben -->
      <span id="luxStatus">–</span>
    </div>

    <div class="status-row">
      <span class="label">RTC (DS3231)</span>
      <!-- initial neutral; wird per JS überschrieben -->
      <span id="rtcStatus">–</span>
    </div>

    <div class="status-row">
      <span class="label">Türposition</span>
      <span>%DOOR_STATE%</span>
    </div>
  </div>

  <!-- ===== NETZWERK ===== -->
  <div class="card">
    <div class="card-title">Netzwerk</div>
    <div class="divider"></div>

    <div class="status-row">
      <span class="label">MQTT</span>
      <span id="mqttStatus">%MQTT_STATUS%</span>
    </div>

    <div class="status-row">
      <span class="label">WLAN Signal</span>
      <span id="rssiStatus">%RSSI% dBm</span>
    </div>

    <div class="status-row">
      <span class="label">IP-Adresse</span>
      <span>%IP%</span>
    </div>
  </div>

  <!-- ===== ACTION ===== -->
  <div class="card">
    <button onclick="location.reload()" class="btn-open">
      🔄 Erneut prüfen
    </button>
  </div>

</div>

<style>
.divider {
  height: 1px;
  background: var(--bg);
  opacity: .6;
  margin: 10px 0 16px 0;
}
.status-row {
  display:flex;
  justify-content:space-between;
  margin-bottom:12px;
  font-size:15px;
}
.label { color: var(--muted); }

.ok    { color: var(--green);  font-weight:600; }
.warn  { color: var(--orange); font-weight:600; }
.error { color: var(--red);    font-weight:600; }

.btn-open { margin-top:10px; background: var(--green); }

</style>

<script>
/* ===== Ampel-Logik (lokal) ===== */
function setClass(el, cls) {
  el.classList.remove("ok","warn","error");
  el.classList.add(cls);
}

function evaluateHeap(){
  const heap = parseInt("%FREE_HEAP%");
  const el = document.getElementById("heapStatus");
  if (!el || Number.isNaN(heap)) return;
  if (heap > 100) setClass(el, "ok");
  else if (heap > 50) setClass(el, "warn");
  else setClass(el, "error");
}

function evaluateRSSI(){
  const rssi = parseInt("%RSSI%");
  const el = document.getElementById("rssiStatus");
  if (!el || Number.isNaN(rssi)) return;
  if (rssi > -65) setClass(el, "ok");
  else if (rssi > -80) setClass(el, "warn");
  else setClass(el, "error");
}

function evaluateMQTT(){
  const el = document.getElementById("mqttStatus");
  const status = "%MQTT_STATUS%";
  if (!el) return;
  if      (status === "Verbunden")   setClass(el, "ok");
  else if (status === "Deaktiviert") setClass(el, "warn");
  else                                setClass(el, "error");
}

/* ===== RTC-/Lux-Setter ===== */
function evaluateRTC(ok, text){
  const el = document.getElementById("rtcStatus");
  if (!el) return;
  el.textContent = text || (ok ? "OK" : "Nicht gefunden / nicht initialisiert");
  setClass(el, ok ? "ok" : "error");
}

/* ✅ Korrigierte Signatur */
function setLuxStatus(isOk, value){
  const el = document.getElementById("luxStatus");
  if (!el) return;

  if (!isOk || !Number.isFinite(value) || value < 0) {
    el.textContent = "Nicht gefunden / nicht initialisiert";
    setClass(el, "error");
  } else {
    const v = typeof value === "number" ? value.toFixed(1) : String(value);
    el.textContent = v + " lx";
    setClass(el, "ok");
  }
}

/* Statische Ampeln (aus Platzhaltern) */
evaluateHeap();
evaluateRSSI();
evaluateMQTT();

/* 🔄 Live-Daten aus /systemtest-status */
fetch("/systemtest-status", { cache: "no-store" })
  .then(r => r.json())
  .then(d => {
    // 🕒 RTC
    const rtcOk  = (d.rtcOk === 1 || d.rtcOk === true);
    const rtcTxt = d.rtcStatus || (rtcOk ? "OK" : "Nicht gefunden / nicht initialisiert");
    evaluateRTC(rtcOk, rtcTxt);

    // 🔆 Lichtsensor
    const bhOk = !!d.bhOk;         // bool
    const lx   = Number(d.lux);    // Zahl oder -1
    setLuxStatus(bhOk && lx >= 0, lx);
  })
  .catch(() => { /* still */ });
</script>

</body>
</html>

)rawliteral";

  html += renderFooter();

  // Platzhalter ersetzen (nur die, die weiterhin statisch angezeigt werden)
  html.replace("%FREE_HEAP%", String(ESP.getFreeHeap() / 1024));
  html.replace("%FW_VERSION%", FW_VERSION);
  // Hinweis: %LUX% wird NICHT mehr verwendet -> Anzeige kommt per JS.
  html.replace("%DOOR_STATE%", doorOpen ? "Offen" : "Geschlossen");
  html.replace("%RSSI%", String(WiFi.RSSI()));
  html.replace("%IP%", WiFi.localIP().toString());

bool mqttConnected = mqttSettings.enabled && mqttClient.connected();

String mqttText;

if (!mqttSettings.enabled) {
  mqttText = "Deaktiviert";
}
else if (mqttConnected) {
  mqttText = "Verbunden";
}
else {
  mqttText = "Nicht verbunden";
}

html.replace("%MQTT_STATUS%", mqttText);

  server.send(200, "text/html; charset=UTF-8", html);
}

/* ===================== LOGBUCH ===================== */
void addLog(String text) {
  String ts;
  if (rtcOk) {
    DateTime now = rtc.now();
    ts = String(now.day()) + "." + String(now.month()) + "." + String(now.year()) + " " +
         String(now.hour()) + ":" + (now.minute() < 10 ? "0" : "") + String(now.minute());
  } else {
    // Fallback, solange die RTC nicht verfügbar/initialisiert ist
    ts = String(millis() / 1000) + "s";
  }

  String entry = ts + " – " + text;

  logbook[logIndex] = entry;
  logIndex = (logIndex + 1) % LOG_SIZE;

  serialLog("📜 LOG: " + entry);

  if (mqttClient.connected()) {
    mqttPublishLog(entry);
  }
}

void serialLog(String msg){
  Serial.println(msg);
}
// Fügt den Lux-Wert nur bei gültigem Wert an, sonst "n/a lx".
// Beispiel: "… (Prognose) – 70.0 lx"
void addLogWithLux(const String& text, float lx) {
  String suffix;
  if (isfinite(lx) && lx >= 0.0f && lx <= 120000.0f) {
    suffix = " – " + String(lx, 1) + " lx";
  } else {
    suffix = " – n/a lx";
  }
  addLog(text + suffix);
}
void clearLogbook() {
  for (int i = 0; i < LOG_SIZE; ++i) {
    logbook[i] = "";
  }
  logIndex = 0;
  Serial.println("📜 LOG: Logbuch wurde gelöscht");
}
// ===== LOG HTML BUILDER =====
String buildLogHTML() {

  String out = "";

  for (int i = 0; i < LOG_SIZE; i++) {

    int idx = (logIndex + i) % LOG_SIZE;
    String line = logbook[idx];

    if (line.length() == 0) continue;

    String cssClass = "log-entry";

    if (line.indexOf("ERROR") >= 0)
      cssClass += " error";
    else if (line.indexOf("WARN") >= 0)
      cssClass += " warn";

    out += "<div class='" + cssClass + "'>";
    out += line;
    out += "</div>";
  }

  if (out.length() == 0)
    out = "<div class='log-entry'>Keine Einträge vorhanden</div>";

  return out;
}

// ===== Logbuch Seite =====
void handleLogbook() {

  String html = renderThemeHead("Logbuch");

  html += R"rawliteral(

<div class="header">
  <h3>📜 Logbuch</h3>
</div>

<div class="container">

  <div class="card">
    <div class="card-title">Ereignisse</div>

    <div id="logContainer" class="log-container">
      %LOG_ENTRIES%
    </div>

    <button class="btn-close" onclick="clearLog()">
      🗑 Log löschen
    </button>
  </div>

</div>

<style>

.log-container {
  max-height: 320px;
  overflow-y: auto;
  margin-top: 10px;
  margin-bottom: 14px;
  padding-right: 4px;
}

/* Einzelner Log-Eintrag */
.log-entry {
  padding: 10px 12px;
  margin-bottom: 8px;
  border-radius: 12px;
  background: var(--bg);
  font-size: 14px;
  line-height: 1.4;
  border-left: 4px solid var(--green);
}

/* Zeit */
.log-time {
  font-size: 12px;
  color: var(--muted);
  display: block;
  margin-bottom: 4px;
}

/* Fehler */
.log-entry.error {
  border-left: 4px solid var(--red);
}

/* Warnung */
.log-entry.warn {
  border-left: 4px solid var(--orange);
}

.btn-close {
  background: var(--red);
}

</style>

<script>

/* Auto-Scroll nach unten */
const logBox = document.getElementById("logContainer");
logBox.scrollTop = logBox.scrollHeight;

function clearLog(){
  fetch("/log/clear",{method:"POST"})
    .then(()=>location.reload());
}

</script>

)rawliteral";

  html += renderFooter();

  // Log HTML einsetzen
  html.replace("%LOG_ENTRIES%", buildLogHTML());

  server.send(200, "text/html; charset=UTF-8", html);
}

// ===== Logbuch Seite =====

void handleFw() {

  String html = renderThemeHead("Firmware Update");

  html += R"rawliteral(

<div class="header">
  <h2>⬆️ Firmware Update</h2>
</div>

<div class="container">

  <div class="card">
   <div style="margin-bottom:12px;font-size:14px;color:var(--muted);">
  Aktuelle Version: <strong>%FW_VERSION%</strong>
</div>
  <div class="warning-box">
  <span class="update-warning">⚠ Während des Updates:</span><br>
  • Gerät nicht ausschalten<br>
  • WLAN nicht trennen<br>
  • Nach erfolgtem Update startet das Gerät neu!
</div><br
 <div class="card-title">Neue Firmware hochladen <i>(z.B. Firmware.bin)</i></div>
   <form id="uploadForm">
      <input type="file" id="file" name="update" accept=".bin" required>
      <button type="submit" class="btn-open">
        Update starten
      </button>
    </form>

    <div id="progressContainer" style="margin-top:15px;display:none;">
      <div style="height:10px;background:var(--bg);border-radius:10px;">
        <div id="progressBar"
             style="height:10px;width:0%;
             background:var(--green);
             border-radius:10px;"></div>
      </div>
      <div id="progressText"
           style="margin-top:8px;font-size:13px;color:var(--muted);">
        0 %
      </div>
    </div>

    <div id="statusMsg"
         style="margin-top:15px;font-weight:600;"></div>
  </div>

</div>
<style>
.update-warning {
  color: var(--red);
  font-weight: 700;
  margin-bottom: 6px;
}

.warning-box {
  background: rgba(239,68,68,0.08);
  padding: 10px 12px;
  border-radius: 10px;
  margin-top: 10px;
}
</style>
<script>

document.getElementById("uploadForm").addEventListener("submit", function(e){
  e.preventDefault();

  const fileInput = document.getElementById("file");
  if(fileInput.files.length === 0){
    alert("Bitte Firmware-Datei auswählen.");
    return;
  }

  const formData = new FormData();
  formData.append("update", fileInput.files[0]);

  const xhr = new XMLHttpRequest();
  xhr.open("POST", "/update", true);   // ⚠ ggf. anpassen falls dein Endpoint anders heißt

  xhr.upload.onprogress = function(e){
    if(e.lengthComputable){
      const percent = Math.round((e.loaded / e.total) * 100);
      document.getElementById("progressContainer").style.display="block";
      document.getElementById("progressBar").style.width = percent + "%";
      document.getElementById("progressText").innerText = percent + " %";
    }
  };

  xhr.onload = function(){
    if(xhr.status == 200){
      document.getElementById("statusMsg").innerHTML =
        "✅ Update erfolgreich. Gerät startet neu...";
      setTimeout(()=>location.href="/",5000);
    } else {
      document.getElementById("statusMsg").innerHTML =
        "❌ Update fehlgeschlagen.";
    }
  };

  xhr.send(formData);
});

</script>

)rawliteral";

  html += renderFooter();
  html.replace("%FW_VERSION%", FW_VERSION);
  server.send(200, "text/html; charset=UTF-8", html);
}


void handleMqtt() {

  String html = renderThemeHead("MQTT Einstellungen");

  html += R"rawliteral(

<div class="header">
  <h3>📡 MQTT Einstellungen</h3>
</div>

<div class="container">
  <div class="card">

    <form method="POST" action="/save-mqtt">

      <div class="section">
        <div class="section-title">🔌 Verbindung</div>
<div class="field">
  <label>
    <input type="checkbox" name="enabled" %MQTT_ENABLED%>
    MQTT aktivieren
  </label>
</div>
        <div class="row">
          <div class="field">
            <label>Broker Host</label>
            <input name="host" value="%HOST%">
          </div>

          <div class="field small">
            <label>Port</label>
            <input name="port" type="number" value="%PORT%">
          </div>
        </div>
      </div>

      <div class="section">
        <div class="section-title">🆔 Identität</div>

        <div class="field">
          <label>Client ID</label>
          <input name="clientId" value="%CLIENTID%">
        </div>

        <div class="field">
          <label>Base Topic</label>
          <input name="base" value="%BASE%">
        </div>

        <div class="field">
  <label>User</label>
  <input name="user" value="%USER%">
</div>

<div class="field">
  <label>Passwort</label>
  <div class="password-wrapper">
    <input id="mqttPass" name="pass" type="password" value="%PASS%">
    <button type="button" onclick="togglePass()" class="eye-btn">👁</button>
  </div>
</div>
<button type="button" onclick="testMqtt()" class="btn-test">
  🧪 Verbindung testen
</button>

<div id="mqttTestResult" class="test-result"></div>
      </div>

      <div class="status-box %MQTT_STATUS_CLASS%">
        %MQTT_STATUS%
      </div>

      <button type="submit" class="btn-open">
        💾 Speichern
      </button>

    </form>
  </div>
</div>

<style>

.section {
  margin-bottom: 20px;
}

.password-wrapper {
  display: flex;
  align-items: center;
  gap: 8px;
}

.password-wrapper input {
  flex: 1;
  padding: 4px 8px !important;
  font-size: 13px;
  }

.eye-btn {
  width: 38px;
  height: 38px;
  border-radius: 12px;
  padding: 0;
  margin: 0;
  background: var(--green);
  font-size: 16px;
}

/* ===== Test Button kleiner ===== */
.btn-test {
  background: var(--orange);
  width: 60%;
  margin: 14px auto 0 auto;
  display: block;
  padding: 8px 12px !important;
  font-size: 14px !important;
  border-radius: 12px;
}
.test-result {
  margin-top: 8px;
  font-size: 13px;
  font-weight: 600;
  text-align: center;
}

.section-title {
  font-size: 13px;
  font-weight: 600;
  text-transform: uppercase;
  margin-bottom: 12px;
  color: var(--muted);
  letter-spacing: .5px;
}

.row {
  display: flex;
  gap: 12px;
}

.field {
  flex: 1;
  display: flex;
  flex-direction: column;
}

.field.small {
  flex: 0 0 100px;
}

label {
  font-size: 13px;
  margin-bottom: 6px;
  color: var(--muted);
}

.status-box {
  margin: 18px 0;
  padding: 10px;
  border-radius: 10px;
  font-size: 14px;
  font-weight: 600;
  text-align: center;
}

.status-ok {
  background: rgba(34,197,94,0.1);
  color: var(--green);
}

.status-error {
  background: rgba(239,68,68,0.1);
  color: var(--red);
}
bool connected = mqttSettings.enabled && mqttClient.connected();

html.replace("%MQTT_STATUS%", 
  !mqttSettings.enabled ? "Deaktiviert" :
  connected ? "Verbunden" : "Nicht verbunden");

html.replace("%MQTT_STATUS_CLASS%",
  !mqttSettings.enabled ? "status-warn" :
  connected ? "status-ok" : "status-error");

</style>

<script>
function testMqtt(){

  const result = document.getElementById("mqttTestResult");
  result.innerHTML = "⏳ Teste Verbindung...";

  const form = document.querySelector("form");
  const formData = new FormData(form);

  fetch("/mqtt-test", {
    method: "POST",
    body: formData
  })
  .then(r => r.text())
  .then(txt => {
    if(txt === "OK"){
      result.innerHTML = "✅ Verbindung erfolgreich";
      result.style.color = "var(--green)";
    } else {
      result.innerHTML = "❌ Verbindung fehlgeschlagen";
      result.style.color = "var(--red)";
    }
  })
  .catch(() => {
    result.innerHTML = "❌ Fehler beim Test";
    result.style.color = "var(--red)";
  });
}
function togglePass(){
  const p = document.getElementById("mqttPass");
  p.type = (p.type === "password") ? "text" : "password";
}
</script>

)rawliteral";
  html += renderFooter();

  html.replace("%HOST%", mqttSettings.host);
  html.replace("%PORT%", String(mqttSettings.port));
  html.replace("%USER%", mqttSettings.user);
  html.replace("%PASS%", mqttSettings.pass);
  html.replace("%CLIENTID%", mqttSettings.clientId);
  html.replace("%BASE%", mqttSettings.base);
  html.replace("%MQTT_ENABLED%", mqttSettings.enabled ? "checked" : "");
  // bool connected = mqttClient.connected();
  bool connected = mqttSettings.enabled && mqttClient.connected();

  html.replace("%MQTT_STATUS%",
               !mqttSettings.enabled ? "Deaktiviert" : connected ? "Verbunden"
                                                                 : "Nicht verbunden");

  html.replace("%MQTT_STATUS_CLASS%",
               !mqttSettings.enabled ? "status-warn" : connected ? "status-ok"
                                                                 : "status-error");

  server.send(200, "text/html; charset=UTF-8", html);
}
void handleSaveMqtt() {

  mqttSettings.enabled = server.hasArg("enabled");
  if (!mqttSettings.enabled) {
    mqttClient.disconnect();
  }

  strncpy(mqttSettings.host, server.arg("host").c_str(), 39);
  mqttSettings.host[39] = '\0';

  mqttSettings.port = server.arg("port").toInt();

  strncpy(mqttSettings.user, server.arg("user").c_str(), 31);
  mqttSettings.user[31] = '\0';

  strncpy(mqttSettings.pass, server.arg("pass").c_str(), 31);
  mqttSettings.pass[31] = '\0';

  strncpy(mqttSettings.clientId, server.arg("clientId").c_str(), 31);
  mqttSettings.clientId[31] = '\0';

  strncpy(mqttSettings.base, server.arg("base").c_str(), 31);
  mqttSettings.base[31] = '\0';

  saveMqttSettings();

  mqttClient.disconnect();
  mqttSetup();

  server.sendHeader("Location", "/mqtt");
  server.send(303);
}

void i2cBusRecover() {

  Serial.println("⚠️ I2C BUS RECOVERY START");

  pinMode(I2C_SDA, INPUT_PULLUP);
  pinMode(I2C_SCL, INPUT_PULLUP);
  delay(10);

  // 9 Clock-Pulse senden um Slave freizugeben
  pinMode(I2C_SCL, OUTPUT);
  for (int i = 0; i < 9; i++) {
    digitalWrite(I2C_SCL, HIGH);
    delayMicroseconds(5);
    digitalWrite(I2C_SCL, LOW);
    delayMicroseconds(5);
  }

  // STOP condition erzeugen
  pinMode(I2C_SDA, OUTPUT);
  digitalWrite(I2C_SDA, LOW);
  delayMicroseconds(5);
  digitalWrite(I2C_SCL, HIGH);
  delayMicroseconds(5);
  digitalWrite(I2C_SDA, HIGH);

  delay(10);

  // Wire neu starten
  Wire.end();
  delay(20);
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(30000);
  Wire.setTimeOut(50);

  Serial.println("✅ I2C BUS RECOVERY DONE");
}

void saveTheme(String theme) {
  uiTheme = theme;
  char buf[10];
  theme.toCharArray(buf, 10);
  EEPROM.put(EEPROM_ADDR_THEME, buf);
  EEPROM.commit();
}
void autoRangeVEML(float luxValue) {

  // zu hell -> weniger empfindlich
  if (luxValue > 20000 && vemlGain != VEML7700_GAIN_1_8) {

    vemlGain = VEML7700_GAIN_1_8;
    veml.setGain(vemlGain);

    Serial.println("VEML Gain -> 1/8");
  }

  // sehr dunkel -> mehr Gain
  else if (luxValue < 10 && vemlGain != VEML7700_GAIN_2) {

    vemlGain = VEML7700_GAIN_2;
    veml.setGain(vemlGain);

    Serial.println("VEML Gain -> 2");
  }

  // mittlere Helligkeit
  else if (luxValue >= 10 && luxValue <= 20000 && vemlGain != VEML7700_GAIN_1) {

    vemlGain = VEML7700_GAIN_1;
    veml.setGain(vemlGain);

    Serial.println("VEML Gain -> 1");
  }
}