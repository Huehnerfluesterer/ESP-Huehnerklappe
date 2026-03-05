## ESP32 Chicken Coop Door Controller

Automatische Hühnerstall-Klappensteuerung mit **ESP32**, **Lichtsensor**, **RTC**, **Motorsteuerung**, **Locklicht** und **Webinterface**.

Das System öffnet und schließt die Klappe automatisch anhand der Helligkeit und bietet zusätzliche Funktionen wie Locklicht, Stallbeleuchtung und manuelle Steuerung.

---

## Hardware

## Mikrocontroller

* ESP32 DevKit V1

Versorgung:

| Pin | Anschluss |
| --- | --------- |
| VIN | 5V        |
| GND | GND       |

---

## Pinbelegung

| ESP32 Pin | Funktion                |
| --------- | ----------------------- |
| GPIO4     | WS2812 LED              |
| GPIO12    | Endschalter geschlossen |
| GPIO14    | Endschalter offen       |
| GPIO18    | Locklicht Relais        |
| GPIO19    | Stalllicht Relais       |
| GPIO21    | I²C SDA                 |
| GPIO22    | I²C SCL                 |
| GPIO25    | Motor IN1               |
| GPIO26    | Motor IN2               |
| GPIO27    | Motor PWM               |
| GPIO32    | Stalllicht Taster       |
| GPIO33    | Tür Taster              |
| GPIO34    | Stromsensor ACS712      |

---

## Motorsteuerung

Motortreiber Beispiel: **L298N**

| ESP32  | Motortreiber |
| ------ | ------------ |
| GPIO25 | IN1          |
| GPIO26 | IN2          |
| GPIO27 | ENA          |

Motoranschluss:

| Motortreiber | Anschluss       |
| ------------ | --------------- |
| OUT1         | Motor           |
| OUT2         | Motor           |
| 12V          | Motorversorgung |

---

## Endschalter

Die Endschalter nutzen **INPUT_PULLUP**.

Logik:

* LOW = aktiv
* HIGH = nicht aktiv

### Öffnungsschalter

GPIO14 → Endschalter → GND

### Schließschalter

GPIO12 → Endschalter → GND

---

## Tür-Taster

GPIO33 → Taster → GND

---

## Stalllicht-Taster

GPIO32 → Taster → GND

---

## Locklicht Relais

GPIO18 → Relais IN
5V → Relais VCC
GND → Relais GND

Relaislogik:

LOW = EIN
HIGH = AUS

---

## Stalllicht Relais

GPIO19 → Relais IN
5V → Relais VCC
GND → Relais GND

---

## WS2812 LED

| ESP32 | LED |
| ----- | --- |
| GPIO4 | DIN |

Zusätzlich empfohlen:

* 330Ω Widerstand in Datenleitung
* 1000µF Kondensator zwischen 5V und GND

---

## Lichtsensor VEML7700

I²C Anschluss:

| ESP32  | Sensor |
| ------ | ------ |
| GPIO21 | SDA    |
| GPIO22 | SCL    |
| 3.3V   | VCC    |
| GND    | GND    |

---

## RTC DS3231

Der RTC nutzt denselben I²C Bus.

| ESP32  | RTC          |
| ------ | ------------ |
| GPIO21 | SDA          |
| GPIO22 | SCL          |
| VCC    | 3.3V oder 5V |
| GND    | GND          |

---

## Stromsensor ACS712

| ESP32  | ACS712 |
| ------ | ------ |
| GPIO34 | OUT    |
| 5V     | VCC    |
| GND    | GND    |

---

## Software Features

* automatische Türöffnung bei Sonnenaufgang
* automatisches Schließen bei Dunkelheit
* Locklicht vor Öffnung
* Locklicht nach Öffnung
* Stallbeleuchtung steuerbar
* Webinterface
* Stromüberwachung des Motors
* Endschalter-Sicherheit
* manuelle Steuerung

---

## Logik

### Öffnen

1. Lichtsensor erkennt steigende Helligkeit
2. optional Locklicht vor Öffnung
3. Motor öffnet Klappe
4. Locklicht nach Öffnung

### Schließen

1. Helligkeit fällt unter Schwelle
2. Motor schließt Klappe
3. Endschalter stoppt Motor

---

## Sicherheit

* Endschalter verhindern Überfahren
* Stromsensor erkennt Blockierung
* Zeitlimits stoppen Motor bei Fehler

---

## Webinterface

Über das Webinterface können folgende Einstellungen angepasst werden:

* Öffnungsschwelle
* Schließschwelle
* Locklichtdauer
* Stalllicht
* manuelles Öffnen und Schließen

---

## Benötigte Bibliotheken

Arduino Libraries:

* WiFi
* WebServer
* Wire
* Adafruit VEML7700
* RTClib
* FastLED

---

## Lizenz

MIT License
