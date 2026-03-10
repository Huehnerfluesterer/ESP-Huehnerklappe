<p align="center">
  <img src="docs/images/esp-huehnerklappe.png" width="900">
</p>

<h1 align="center">🐔 ESP32 Hühnerstall-Tür Controller</h1>
<p align="center">
  Lux-Automatik &nbsp;•&nbsp; Sonnenuntergangs-Prognose &nbsp;•&nbsp; MQTT &nbsp;•&nbsp; Web-Interface &nbsp;•&nbsp; OTA-Updates
</p>

<p align="center">
  <img src="https://img.shields.io/badge/ESP32-Kompatibel-blue" />
  <img src="https://img.shields.io/badge/PlatformIO-Projekt-orange" />
  <img src="https://img.shields.io/badge/Firmware-v1.0.15-brightgreen" />
  <img src="https://img.shields.io/badge/MQTT-Unterst%C3%BCtzt-green" />
  <img src="https://img.shields.io/badge/Lizenz-MIT-yellow" />
</p>

---

Ein **ESP32-basierter intelligenter Controller für automatische Hühnerstalltüren**, der Umgebungslicht misst, Sonnenuntergänge vorhersagt und mehrere Sicherheits-Fallbacks enthält.

Der Controller verwendet einen **VEML7700 Lux-Sensor** und kombiniert Medianfilter, exponentielle Glättung und Trendalgorithmen, um den optimalen Zeitpunkt zum Öffnen und Schließen der Tür zu bestimmen – auch bei Wolken, Regen, Schnee und Sensorrauschen.

---

## 🌍 Sprachen

🇬🇧 [English](README.md)  
🇩🇪 Deutsch

---

## ✨ Hauptfunktionen

| Funktion | Beschreibung |
|---|---|
| 🌅 **Lux-Automatik** | Öffnen/Schließen anhand Umgebungshelligkeit (VEML7700) |
| 📉 **Sonnenuntergangs-Prognose** | Schließzeitpunkt aus Lux-Änderungsrate berechnet |
| 💡 **Vorlicht** | Stalllicht geht vor dem Schließen an, damit Hühner reinkommen |
| 🌥 **Wolken-Erkennung** | Verhindert Fehltrigger durch Wolken oder kurze Aufhellung |
| ⏰ **Zeit-Fallback** | Wechselt auf Zeitsteuerung wenn Sensor ausfällt |
| 🛑 **Endschalter** | Optionale Hardware-Positionserkennung |
| 📡 **MQTT** | Vollständige Home-Assistant / Node-RED Integration |
| 🌐 **Web-Interface** | Steuerung und Konfiguration im Browser |
| 🔧 **OTA-Updates** | Firmware-Update über WLAN |
| 📜 **Logbuch** | Ereignisprotokoll im EEPROM gespeichert |
| 🔴 **RGB-Beleuchtung** | PWM-gesteuerter LED-Strip über N-Kanal MOSFETs |

---

## 🌅 Lux-basierte Automatik

Die Tür wird vollautomatisch anhand der Umgebungshelligkeit gesteuert. Jeder Messwert durchläuft eine mehrstufige Verarbeitungs-Pipeline:

1. Rohwert-Messung mit automatischer Verstärkungs-Umschaltung
2. Medianfilter (Puffergröße 5) gegen Spitzen
3. Exponentielles Glätten (EMA) für stabile Werte
4. Lux-Trend-Berechnung (Änderungsrate über 30 s)
5. Prognose-Logik für Sonnenuntergang

Damit werden **Sonnenauf- und -untergang zuverlässig erkannt**, auch bei bewölktem Himmel.

---

## 📉 Sonnenuntergangs-Prognose

Die Firmware berechnet die **Lux-Änderungsrate**, um vorherzusagen, wann die Schließschwelle erreicht wird:

```
minutesToThresh = (lux - threshold) / luxRate
```

Das System bereitet das Schließen **vor** dem tatsächlichen Schwellwert vor – für präziseres Timing und ruhigeres Dämmerungsverhalten.

---

## 💡 Vorlicht vor dem Schließen

Bevor die Tür schließt, wird das Stalllicht für eine konfigurierbare Zeit eingeschaltet, damit alle Hühner hereinkommen. Wird es zwischendurch wieder heller (Wolke), wird die Vorlicht-Phase **automatisch abgebrochen**.

---

## 🌥 Wolken-Erkennung

Kurzzeitige Aufhellungen lösen keinen Fehlalarm aus:

```
lux > closeThreshold + PRECLOSE_ABORT_MARGIN_LX
```

Verhindert vorzeitiges Schließen durch vorbeiziehende Wolken.

---

## 🔄 Fallback-Systeme

### ⏰ Zeitbasierter Fallback

Fällt der Lux-Sensor aus, wechselt das System automatisch auf **Zeitsteuerung**. Auslöser:

- Sensorfehler
- Ungültige Lux-Werte
- I²C-Kommunikationsfehler
- Sensor-Timeout

### 📡 Sensor-Gesundheitsüberwachung

Laufende Überprüfung von Lux-Gültigkeit, I²C-Kommunikation und Antwortzeiten mit automatischer Wiederherstellung.

### 🔌 I²C-Bus-Recovery

Bei Bus-Blockade: 9 Clock-Pulse + STOP-Bedingung, gefolgt von Sensor-Neuinitialisierung und automatischer Wiederverbindung.

---

## 🛑 Sicherheitsfunktionen

**Endschalter** — Erkennen „Tür vollständig offen" und „Tür vollständig geschlossen". Schützen den Motor vor Überlastung. Im Web-Interface aktivierbar.

**Nacht-Sperre** — Sobald die Tür nachts geschlossen ist, wird erneutes Öffnen blockiert bis zum Morgenfenster. Schützt vor Fehltriggern durch Außenbeleuchtung.

**Glitch-Filter** — Kurze Helligkeitsspitzen durch Blitze, Autoscheinwerfer oder Kamerablitze werden per Lux-Hysterese und Hold-Timer ignoriert.

**Motorblockade-Erkennung** — ACS712-Stromsensor erkennt Blockaden. Bei Blockade fährt der Motor kurz zurück.

---

## 🌐 Konnektivität

### MQTT

Vollständige Steuerung und Status per MQTT, kompatibel mit Home Assistant, Node-RED und anderen Systemen. Siehe [MQTT-Dokumentation](docs/mqtt.de.md).

### Web-Interface

Integrierter Webserver auf Port 80, erreichbar von jedem Browser im Heimnetz. Als **PWA** auf dem Smartphone installierbar.

| Route | Funktion |
|---|---|
| `/` | Dashboard: Status, Türsteuerung |
| `/settings` | Automatik-Einstellungen |
| `/mqtt` | MQTT-Konfiguration |
| `/learn` | Motorposition einlernen |
| `/log` | Logbuch |
| `/systemtest` | Hardware-Selbsttest |
| `/fw` | OTA Firmware-Update |

---

## 🛠 Hardware

Typischer Aufbau:

- ESP32 DevKit v1
- VEML7700 Lichtsensor (I²C)
- DS3231 Echtzeituhr (I²C)
- L298N Motortreiber
- 12 V DC Türmotor
- 2× Relaismodul (Locklicht, Stalllicht)
- 3× Taster
- Optional: Endschalter, ACS712 Stromsensor, RGB LED-Strip mit IRLZ44N MOSFETs

Vollständige Verkabelungsanleitung: [Hardware & Verkabelung](docs/HW.de.md)

---

## 📦 Installation

Schritt-für-Schritt Installationsanleitung:

🇬🇧 [Installation Guide (English)](docs/installation.md)  
🇩🇪 [Installationsanleitung (Deutsch)](docs/installation.de.md)

---

## 📷 Hardware-Verkabelung

![Wiring](docs/images/wiring.png)

---

## 🧠 System-Architektur

![Architecture](docs/images/architecture.png)

---

## 📜 Lizenz

MIT License – Freie Nutzung, Veränderung und Weitergabe erlaubt.

---

## 🤝 Mitarbeit

Pull Requests und Verbesserungsvorschläge sind willkommen.
