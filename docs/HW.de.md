## ESP32 Chicken Coop Door Controller 🐔

Automatische Hühnerstall-Klappensteuerung mit **ESP32**, **Lichtsensor**, **RTC**, **Motorsteuerung**, **Locklicht** und **Webinterface**.

Das System öffnet und schließt die Stallklappe automatisch abhängig von der Helligkeit.

---

## 📷 Systemübersicht

```mermaid
flowchart LR

Sun[Sunrise / Sunset]
LightSensor[VEML7700 Light Sensor]
ESP[ESP32 Controller]
MotorDriver[Motor Driver]
Door[Chicken Coop Door]

RTC[RTC DS3231]
Buttons[Manual Buttons]
Relays[Relays]
Lights[LED / Coop Light]

Sun --> LightSensor
LightSensor --> ESP
RTC --> ESP
Buttons --> ESP

ESP --> MotorDriver
MotorDriver --> Door

ESP --> Relays
Relays --> Lights
```

---

## 🧠 Funktionsprinzip

```mermaid
flowchart TD

Lux[Lichtsensor misst Helligkeit]
Check{Schwellenwert erreicht?}

PreLight[Locklicht vor Öffnung]
OpenDoor[Motor öffnet Klappe]

PostLight[Locklicht nach Öffnung]

CloseCheck{Dunkelheit erreicht?}
CloseDoor[Motor schließt Klappe]

Lux --> Check

Check -->|Ja| PreLight
PreLight --> OpenDoor
OpenDoor --> PostLight

Lux --> CloseCheck
CloseCheck -->|Ja| CloseDoor
```

---

## 🔌 Hardwareübersicht

## Hauptkomponenten

| Bauteil              | Beschreibung      |
| -------------------- | ----------------- |
| ESP32 DevKit         | Hauptcontroller   |
| VEML7700             | Helligkeitssensor |
| DS3231               | Echtzeituhr       |
| L298N / Motor Driver | Motorsteuerung    |
| ACS712               | Stromsensor       |
| WS2812               | Locklicht         |
| Relais               | Stallbeleuchtung  |
| Endschalter          | Türposition       |

---

## ⚡ Pinbelegung

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
| GPIO34    | Stromsensor             |

---

## 🔧 Verdrahtungsdiagramm

```mermaid
flowchart LR

ESP32 --- VEML7700
ESP32 --- DS3231
ESP32 --- Buttons
ESP32 --- MotorDriver
ESP32 --- Relays
ESP32 --- WS2812
ESP32 --- ACS712

MotorDriver --- Motor
Motor --- Door

Relays --- CoopLight
WS2812 --- LockLight

Buttons --- ManualControl
```

---

## 🔌 I²C Bus

Der I²C Bus wird von mehreren Komponenten gemeinsam genutzt.

```
ESP32 GPIO21 (SDA) ───── VEML7700 SDA
                       └──── DS3231 SDA

ESP32 GPIO22 (SCL) ───── VEML7700 SCL
                       └──── DS3231 SCL
```

---

## 🚪 Endschalter

Endschalter arbeiten mit **INPUT_PULLUP**.

Logik:

```
LOW  = Endschalter aktiv
HIGH = Endschalter nicht aktiv
```

Verdrahtung:

```
GPIO14 ─── Endschalter offen ─── GND
GPIO12 ─── Endschalter geschlossen ─── GND
```

---

## 🔘 Taster

Türsteuerung:

```
GPIO33 ─── Taster ─── GND
```

Stalllicht:

```
GPIO32 ─── Taster ─── GND
```

---

## 💡 Locklicht (WS2812)

```
GPIO4 ─── 330Ω ─── DIN WS2812
5V ─────────────── VCC
GND ────────────── GND
```

Empfohlen:

* 330Ω Datenwiderstand
* 1000µF Kondensator zwischen 5V und GND

---

## ⚙️ Motorsteuerung

Beispiel mit **L298N**

```
ESP32 GPIO25 → IN1
ESP32 GPIO26 → IN2
ESP32 GPIO27 → ENA

Motor → OUT1 / OUT2
12V → Motorversorgung
```

---

## 🔒 Sicherheit

Das System enthält mehrere Sicherheitsfunktionen:

* Endschalter stoppen den Motor
* Stromsensor erkennt Blockierungen
* Zeitlimit verhindert Dauerlauf
* Manuelle Steuerung jederzeit möglich

---

## 🌐 Webinterface

Über das Webinterface können eingestellt werden:

* Öffnungsschwelle
* Schließschwelle
* Locklichtdauer
* Stalllicht
* manuelle Türsteuerung

---

## 📦 Projektstruktur

Empfohlene GitHub Struktur:

```
/firmware
/docs
/images
/hardware
README.md
```

---

## 📜 Lizenz

MIT License
