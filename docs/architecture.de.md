# 🧠 System-Architektur

🇬🇧 [English](architecture.md) &nbsp;|&nbsp; 🇩🇪 Deutsch

---

## Inhaltsverzeichnis

- [Überblick](#-überblick)
- [Projektstruktur](#-projektstruktur)
- [Lux-Verarbeitungs-Pipeline](#-lux-verarbeitungs-pipeline)
- [Zustandsmaschinen](#-zustandsmaschinen)
- [Automatik-Logik](#-automatik-logik)
- [Loop-Architektur](#-loop-architektur)

---

## 🗺 Überblick

Das System ist modular aufgebaut. Jede Komponente hat eine klar abgegrenzte Verantwortlichkeit:

```
┌─────────────────────────────────────────────────────────────────┐
│                          main.cpp                               │
│          Setup · Loop · WebServer-Routen · OTA                  │
└───────┬──────────────────────────────────────────────┬──────────┘
        │                                              │
   ┌────▼─────┐  ┌──────────┐  ┌──────────┐  ┌───────▼──────┐
   │  logic   │  │   lux    │  │  motor   │  │     web/     │
   │ Automatik│  │ VEML7700 │  │  L298N   │  │  Webserver   │
   └────┬─────┘  └──────────┘  └──────────┘  └──────────────┘
        │
   ┌────▼─────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐
   │  light   │  │   door   │  │   mqtt   │  │   storage    │
   │  Licht   │  │  Taster  │  │  MQTT    │  │   EEPROM     │
   └──────────┘  └──────────┘  └──────────┘  └──────────────┘
        │
   ┌────▼─────┐  ┌──────────┐
   │  system  │  │  logger  │
   │  Health  │  │ Logbuch  │
   └──────────┘  └──────────┘
```

---

## 📁 Projektstruktur

```
ESP-Huehnerklappe/
├── platformio.ini              # Build-Konfiguration, Bibliotheken
└── src/
    ├── config.h                # ⚠️ WLAN-Zugangsdaten (in .gitignore!)
    ├── pins.h                  # Alle GPIO-Pin-Definitionen
    ├── types.h                 # Enums (DoorPhase, MotorState, LightState)
    │                           # und Structs (Settings, MqttSettings)
    ├── main.cpp                # Setup, Loop, alle WebServer-Routen, OTA
    ├── motor.cpp / motor.h     # L298N-Ansteuerung via LEDC-PWM
    ├── door.cpp  / door.h      # Türzustand, Taster-Handling
    ├── lux.cpp   / lux.h       # VEML7700: Sensor, Filter, Trend, Recovery
    ├── light.cpp / light.h     # Licht-Automatik, RGB-PWM, Dimmer
    ├── logic.cpp / logic.h     # Automatik-Entscheidungslogik
    ├── mqtt.cpp  / mqtt.h      # PubSubClient-Wrapper, Topics, Publish
    ├── wlan.cpp  / wlan.h      # WLAN-Verbindung, Watchdog
    ├── storage.cpp / storage.h # EEPROM-Lesen/Schreiben
    ├── logger.cpp  / logger.h  # Ringpuffer-Logbuch im EEPROM
    ├── system.cpp  / system.h  # System-Health, Uptime
    ├── icons.h                 # PWA-Icons als PROGMEM-Arrays
    └── web/
        ├── web.h               # Webserver-Header, Routing
        ├── web_root.cpp        # Startseite (Dashboard)
        ├── web_pages.cpp       # Systemtest, Logbuch, Firmware-Update
        ├── web_settings.cpp    # Einstellungs- und MQTT-Seite
        └── web_helpers.cpp     # HTML-Bausteine, CSS, gemeinsame Elemente
```

---

## 🔬 Lux-Verarbeitungs-Pipeline

Jeder Lux-Rohwert durchläuft eine mehrstufige Pipeline, bevor er für Entscheidungen verwendet wird:

```
VEML7700 Hardware
      │
      ▼
┌─────────────┐
│ Rohwert     │  getLux() – direkte I²C-Abfrage
│ Messung     │  Fehler → NAN
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Median-     │  medianLux(rawValue)
│ Filter      │  Puffergröße 5, sortierte Auswahl des Mittenwerts
│             │  Filtert: Spitzen, Einzelausreißer
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ EMA-Filter  │  lux = lux * 0.8 + raw * 0.2
│ (exponential│  Glättet: kurze Schwankungen, Rauschen
│ moving avg) │
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Trend-      │  updateLuxTrend() alle 30 s
│ Berechnung  │  luxRate = (lux - lastLux) / deltaTime
│             │  → lx/min Änderungsrate
└──────┬──────┘
       │
       ▼
┌─────────────┐
│ Prognose    │  runAutomatik()
│ Logik       │  minutesToThresh = (lux - threshold) / luxRate
│             │  Entscheidung: Vorlicht starten / Schließen
└─────────────┘
```

---

## 🔄 Zustandsmaschinen

### Türzustand (DoorPhase)

```
         ┌─────────────────────────────────────┐
         │                                     │
    ─────▼─────                          ──────┴──────
    │  IDLE   │ ──── Öffnen ausgelöst ──► │ OPENING  │
    │         │                           │          │
    └────┬────┘                           └────┬─────┘
         ▲                                     │ Endposition / Timeout
         │                              ───────▼──────
    Schließen                           │    OPEN    │
    abgeschlossen                       │            │
         │                              └────┬───────┘
    ─────┴───────                            │ Schließen ausgelöst
    │ CLOSING  │ ◄─── Schließen ausgelöst ───┘
    │          │
    └──────────┘
```

### Motorzustand (MotorState)

```
MOTOR_STOPPED ◄──── Timeout / Endschalter / Stop-Befehl
      │
      ├──── startMotorOpen()  ────► MOTOR_OPENING
      └──── startMotorClose() ────► MOTOR_CLOSING
```

### Lichtzustand (LightState)

```
LIGHT_OFF
   │
   ├──► LIGHT_PRE_OPEN    (Vorlicht vor Türöffnung)
   │         │
   │         ▼
   │    LIGHT_POST_OPEN   (Nachlicht nach Öffnung)
   │         │
   │         ▼
   │    LIGHT_OFF
   │
   └──► LIGHT_PRE_CLOSE   (Vorlicht vor Türschließung, mit Wolken-Abort)
             │
             ▼
        LIGHT_POST_CLOSE  (Nachlicht nach Schließung, mit Dimmen)
             │
             ▼
        LIGHT_OFF
```

---

## 🤖 Automatik-Logik

Die Funktion `runAutomatik()` in `logic.cpp` wird alle 200 ms aufgerufen und trifft alle automatischen Entscheidungen:

```
runAutomatik()
│
├── Modus = "time"?
│     └── Uhrzeitvergleich → Öffnen / Schließen
│
├── Modus = "lux"?
│     ├── luxReady && luxValid?
│     │     ├── lux > openThreshold → Tür öffnen (mit Hysterese + Hold-Timer)
│     │     ├── lux < closeThreshold
│     │     │     ├── Prognose aktiv? → minutesToThresh berechnen
│     │     │     ├── Vorlicht starten (PRE_CLOSE)
│     │     │     └── lux stabil < Schwelle → Schließen auslösen
│     │     └── Wolken-Abort: lux steigt → PRE_CLOSE abbrechen
│     │
│     └── luxReady = false → Fallback auf Zeitsteuerung
│
├── Nacht-Sperre aktiv? → kein Öffnen
├── manualOverrideUntil > jetzt? → Automatik pausiert
└── actionLock? → warten bis Motor stoppt
```

---

## ⏱ Loop-Architektur

Der Haupt-Loop in `main.cpp` läuft alle **200 ms** (konfigurierbar via `LOGIC_INTERVAL`):

```
loop()
│
├── server.handleClient()        ← WebServer, immer
│
├── if (otaInProgress) return    ← OTA hat Vorrang
│
├── mqttLoop()                   ← MQTT keep-alive + empfangen
├── wifiWatchdog()               ← Reconnect bei Verbindungsverlust
│
├── updateMotor()                ← Timeout + Endschalter prüfen
├── updateButton()               ← Taster Tür
├── updateStallButton()          ← Taster Stalllicht
├── updateRedButton()            ← Taster Rotlicht
│
├── getLux() (alle 1 s)          ← Sensor lesen
├── medianLux()                  ← Medianfilter
├── EMA-Filter                   ← Exponentielles Glätten
├── checkLuxHealth()             ← Fehlerüberwachung + Recovery
├── updateLuxTrend() (alle 30 s) ← Änderungsrate berechnen
│
├── updateSystemHealth()         ← Heap, Uptime, Sensor-Status
├── updateDimming()              ← RGB-Dimmer aktualisieren
├── updateStallLightTimer()      ← Stalllicht-Timer
│
├── runAutomatik()               ← Automatik-Logik (nur mit RTC)
└── updateLightState()           ← Lichtzustandsmaschine
```

> Während eines OTA-Updates wird nur `server.handleClient()` aufgerufen – alle anderen Aktionen sind blockiert, der Motor ist gestoppt, alle Ausgänge sind aus.
