<p align="center">
  <img src="docs/images/banner.png" width="900">
</p>

<h1 align="center">🐔 ESP32 Chicken Coop Door Controller</h1>
<p align="center">
Lux-based automation • Sunset prediction • MQTT integration
</p>
# 🐔 Smarter Hühnerstall-Tür Controller

[![ESP32](https://img.shields.io/badge/ESP32-Kompatibel-blue)]()
[![PlatformIO](https://img.shields.io/badge/PlatformIO-Projekt-orange)]()
[![MQTT](https://img.shields.io/badge/MQTT-Unterstützt-green)]()
[![Lizenz](https://img.shields.io/badge/Lizenz-MIT-yellow)]()

Ein **ESP32-basierter intelligenter Controller für automatische Hühnerstalltüren**, der Umgebungslicht misst, Sonnenuntergänge vorhersagt und mehrere Sicherheits-Fallbacks enthält.

Der Controller verwendet einen **VEML7700 Lux-Sensor** und kombiniert mehrere Filter- und Trendalgorithmen, um den optimalen Zeitpunkt zum Öffnen und Schließen der Tür zu bestimmen.

Er wurde für **reale Umweltbedingungen** entwickelt, z. B.:

* Wolken
* Regen
* Schneereflexion
* schnelle Lichtänderungen
* Sensorsignale mit Rauschen

---

# 🌍 Sprachen

🇬🇧 [English](README.md)
🇩🇪 Deutsch

---

# ✨ Hauptfunktionen

## 🌅 Lichtbasierte Automatik

Die Tür wird automatisch anhand der Umgebungshelligkeit gesteuert.

Verwendete Techniken:

* VEML7700 Lux-Sensor
* automatische Sensorverstärkung
* Median-Filter
* exponentielle Glättung
* Lux-Trend-Erkennung

Damit können **Sonnenaufgang und Sonnenuntergang zuverlässig erkannt werden**, auch bei bewölktem Himmel.

---

## 📉 Sonnenuntergangs-Prognose

Die Firmware berechnet die **Änderungsrate der Helligkeit**, um vorherzusagen, wann die Schließschwelle erreicht wird.

```
minutesToThresh = (lux - threshold) / luxRate
```

Vorteile:

* frühere Vorbereitung auf das Schließen
* stabileres Verhalten während der Dämmerung
* präzisere Schließzeitpunkte

---

## 💡 Vorlicht vor dem Schließen

Vor dem Schließen wird das Stalllicht für eine einstellbare Zeit eingeschaltet.

Zweck:

* Hühner sammeln sich im Stall
* sicherer Schließvorgang
* bessere Zuverlässigkeit im Winter

---

## 🌥 Wolken-Erkennung

Wenn es kurzfristig wieder heller wird, wird die Schließ-Prognose abgebrochen:

```
lux > closeThreshold + PRECLOSE_ABORT_MARGIN_LX
```

Dadurch werden **Fehltrigger durch Wolken verhindert**.

---

# 🔄 Fallback-Systeme

## ⏰ Zeitbasierter Fallback

Falls der Lux-Sensor ausfällt, schaltet das System automatisch auf **zeitbasierte Steuerung** um.

Mögliche Ursachen:

* Sensorfehler
* ungültige Lux-Werte
* I²C-Kommunikationsfehler
* Sensor-Timeout

---

## 📡 Sensorüberwachung

Der Sensorzustand wird ständig überprüft:

* Lux-Validität
* I²C-Kommunikation
* Antwortzeiten
* automatische Wiederherstellung

---

## 🔌 I²C-Bus-Recovery

Wenn der I²C-Bus blockiert ist:

* Bus-Reset
* Sensor-Neuinitialisierung
* automatische Wiederverbindung

---

# 🛑 Sicherheitsfunktionen

## Endschalter

Endschalter erkennen:

* Tür vollständig geöffnet
* Tür vollständig geschlossen

Dadurch werden **Motorschäden und mechanische Belastungen verhindert**.

---

## Nacht-Sperre

Wenn die Tür nachts geschlossen ist:

* wird erneutes Öffnen verhindert
* Lichtquellen lösen keinen Fehltrigger aus
* Reset erfolgt nur morgens

---

## Glitch-Filter

Kurze Helligkeitsspitzen werden gefiltert, z. B.:

* Blitze
* Autoscheinwerfer
* Kamerablitze
* kurze Schatten

---

# 🌐 Konnektivität

## MQTT

Status und Steuerung über MQTT.

Integration mit:

* Home Assistant
* Node-RED
* Smart-Home-Systemen

---

# 🖥 Weboberfläche

Integrierte Weboberfläche für:

* Konfiguration
* manuelle Türsteuerung
* Systemstatus
* Log-Anzeige

---

# 🔧 OTA Updates

Firmware kann **over-the-air aktualisiert werden**, ohne physischen Zugriff.

---

# 📜 Ereignisprotokoll

Das System protokolliert:

* Türbewegungen
* Lichtbedingungen
* Prognoseereignisse
* Fallback-Aktivierungen
* Sensorfehler

---

# 🛠 Hardware

Typischer Aufbau:

* ESP32
* VEML7700 Lux-Sensor
* Motortreiber
* Türmotor
* Endschalter
* Stalllicht

---

# 📜 Lizenz

MIT License
