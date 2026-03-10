# 📡 MQTT-Dokumentation

🇬🇧 [English](mqtt.md) &nbsp;|&nbsp; 🇩🇪 Deutsch

---

## Inhaltsverzeichnis

- [Konfiguration](#-konfiguration)
- [Topics – Überblick](#-topics--überblick)
- [Status-Payload](#-status-payload)
- [Befehle (cmd)](#-befehle-cmd)
- [Einstellungen per MQTT](#-einstellungen-per-mqtt)
- [Home Assistant](#-home-assistant-integration)

---

## ⚙️ Konfiguration

MQTT wird im Web-Interface unter **`/mqtt`** konfiguriert:

| Feld | Beschreibung | Beispiel |
|---|---|---|
| **Aktiviert** | MQTT ein/ausschalten | ✅ |
| **Host** | IP oder Hostname des Brokers | `192.168.1.100` |
| **Port** | Standard MQTT-Port | `1883` |
| **Client-ID** | Eindeutiger Name des Geräts | `huehnerklappe` |
| **Base-Topic** | Präfix für alle Topics | `huehnerklappe` |
| **Benutzer** | Optional, falls Broker Authentifizierung verlangt | `mqtt_user` |
| **Passwort** | Optional | `geheim` |

Nach dem Speichern verbindet sich der ESP32 automatisch mit dem Broker. Mit dem Button **„Verbindung testen"** kann die Verbindung vorab geprüft werden.

---

## 📋 Topics – Überblick

Alle Topics verwenden den konfigurierten **Base-Topic** als Präfix (Standard: `huehnerklappe`).

### Ausgehend (ESP32 → Broker)

| Topic | Inhalt | Typ |
|---|---|---|
| `{base}/available` | `online` / `offline` | String |
| `{base}/status` | Vollständiger Gerätestatus | JSON |
| `{base}/settings` | Aktuelle Einstellungen | JSON (retained) |
| `{base}/log` | Neue Logbuch-Einträge | String |

### Eingehend (Broker → ESP32)

| Topic | Inhalt | Typ |
|---|---|---|
| `{base}/cmd` | Steuerbefehl | JSON |

---

## 📊 Status-Payload

Das Topic `{base}/status` wird bei jeder Statusänderung veröffentlicht.

**Beispiel:**

```json
{
  "door": "open",
  "doorPhase": "OPEN",
  "motor": "STOPPED",
  "motorReason": "Automatik/Lux",
  "lux": 342.5,
  "luxReady": true,
  "luxRate": -12.3,
  "lightActive": false,
  "stallLightActive": true,
  "rgbRedActive": false,
  "wifiRssi": -58,
  "mqttConnected": true,
  "rtcOk": true,
  "vemlOk": true,
  "uptime": 86400,
  "fwVersion": "1.0.15"
}
```

| Feld | Typ | Beschreibung |
|---|---|---|
| `door` | String | `open` / `closed` |
| `doorPhase` | String | `IDLE` / `OPENING` / `OPEN` / `CLOSING` |
| `motor` | String | `STOPPED` / `OPENING` / `CLOSING` |
| `motorReason` | String | Auslöser der letzten Motoraktion |
| `lux` | Float | Aktueller gefilterter Lux-Wert |
| `luxReady` | Bool | Sensor hat stabilen Wert geliefert |
| `luxRate` | Float | Lux-Änderungsrate (lx/min) |
| `lightActive` | Bool | Locklicht aktiv |
| `stallLightActive` | Bool | Stalllicht aktiv |
| `rgbRedActive` | Bool | RGB-Rotlicht aktiv |
| `wifiRssi` | Int | WLAN-Signalstärke in dBm |
| `rtcOk` | Bool | RTC DS3231 korrekt erkannt |
| `vemlOk` | Bool | VEML7700 korrekt erkannt |
| `uptime` | Int | Laufzeit in Sekunden |
| `fwVersion` | String | Aktuell installierte Firmware-Version |

---

## 🎮 Befehle (cmd)

Steuerung des Geräts über das Topic **`{base}/cmd`** mit JSON-Payload:

### Türsteuerung

```json
{ "door": "open" }
{ "door": "close" }
{ "door": "stop" }
```

### Lichtsteuerung

```json
{ "light": "on" }
{ "light": "off" }
{ "stalllight": "on" }
{ "stalllight": "off" }
{ "rgbred": "on" }
{ "rgbred": "off" }
```

### System

```json
{ "reset": true }
```

---

## 📝 Einstellungen per MQTT

Einstellungen können auch über MQTT übermittelt werden (gleiche Struktur wie `/settings`-Seite):

```json
{
  "openMode": "lux",
  "closeMode": "lux",
  "openTime": "07:00",
  "closeTime": "21:00",
  "openLightThreshold": 200,
  "closeLightThreshold": 20,
  "lampPreOpen": 10,
  "lampPostOpen": 5,
  "lampPreClose": 15,
  "lampPostClose": 5
}
```

---

## 🏠 Home Assistant Integration

### Manuelle Konfiguration (configuration.yaml)

```yaml
mqtt:
  # Tür
  cover:
    - name: "Hühnerklappe"
      unique_id: huehnerklappe_door
      command_topic: "huehnerklappe/cmd"
      state_topic: "huehnerklappe/status"
      value_template: "{{ value_json.door }}"
      payload_open: '{"door":"open"}'
      payload_close: '{"door":"close"}'
      payload_stop: '{"door":"stop"}'
      state_open: "open"
      state_closed: "closed"
      availability_topic: "huehnerklappe/available"
      device_class: gate

  # Locklicht
  light:
    - name: "Locklicht"
      unique_id: huehnerklappe_light
      command_topic: "huehnerklappe/cmd"
      state_topic: "huehnerklappe/status"
      value_template: "{{ value_json.lightActive }}"
      payload_on: '{"light":"on"}'
      payload_off: '{"light":"off"}'
      state_on: true
      state_off: false

  # Stalllicht
    - name: "Stalllicht"
      unique_id: huehnerklappe_stalllight
      command_topic: "huehnerklappe/cmd"
      state_topic: "huehnerklappe/status"
      value_template: "{{ value_json.stallLightActive }}"
      payload_on: '{"stalllight":"on"}'
      payload_off: '{"stalllight":"off"}'
      state_on: true
      state_off: false

  # Sensoren
  sensor:
    - name: "Helligkeit Hühnerstall"
      unique_id: huehnerklappe_lux
      state_topic: "huehnerklappe/status"
      value_template: "{{ value_json.lux | round(1) }}"
      unit_of_measurement: "lx"
      device_class: illuminance

    - name: "Hühnerklappe Firmware"
      unique_id: huehnerklappe_fw
      state_topic: "huehnerklappe/status"
      value_template: "{{ value_json.fwVersion }}"
      icon: mdi:chip

  binary_sensor:
    - name: "Hühnerklappe Online"
      unique_id: huehnerklappe_available
      state_topic: "huehnerklappe/available"
      payload_on: "online"
      payload_off: "offline"
      device_class: connectivity
```

> 💡 Mit **MQTT Discovery** (falls aktiviert) werden Entitäten in Home Assistant automatisch erkannt, ohne manuelle Konfiguration.
