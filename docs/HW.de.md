# 🔌 Hardware & Verkabelung

🇬🇧 [English](HW.md) &nbsp;|&nbsp; 🇩🇪 Deutsch

---

## Inhaltsverzeichnis

- [Benötigte Bauteile](#-benötigte-bauteile)
- [Pin-Übersicht ESP32](#-pin-übersicht-esp32)
- [Schritt-für-Schritt Verkabelung](#-schritt-für-schritt-verkabelung)
- [Steckverbindungen im Überblick](#-steckverbindungen-im-überblick)

---

## 🛒 Benötigte Bauteile

### Pflichtbauteile

| Bauteil | Beschreibung | Anzahl |
|---|---|---|
| **ESP32 DevKit v1** | Mikrocontroller (WROOM-32) | 1 |
| **L298N** | Motorsteuermodul (Dual H-Brücke) | 1 |
| **Gleichstrommotor 12 V** | Passend zur Klappe (Getriebemotor empfohlen) | 1 |
| **VEML7700** | Lichtsensor-Breakout (I²C, 3,3 V) | 1 |
| **DS3231** | Echtzeituhr-Modul (I²C, mit CR2032-Akku) | 1 |
| **Relaismodul 5 V, 2-Kanal** | LOW-aktiv | 1 |
| **Netzteil 12 V / min. 2 A** | Für Motor und Lampen | 1 |
| **Netzteil 5 V** | Für ESP32 (USB oder Step-Down von 12 V) | 1 |
| **Taster NO** | Kurzhubtaster, normalerweise offen | 3 |
| **Jumperkabel** | Stecker-Buchse, verschiedene Längen | ~30 |

### Optionale Bauteile

| Bauteil | Beschreibung | Anzahl |
|---|---|---|
| **IRLZ44N** | N-Kanal MOSFET für RGB LED-Strip | 3 |
| **Widerstand 470 Ω** | Gate-Vorwiderstand für MOSFET | 3 |
| **RGB LED-Strip 12 V** | Getrennte R/G/B-Kanäle (kein WS2812!) | nach Bedarf |
| **ACS712 (5 A oder 20 A)** | Stromsensor für Motorblockade-Erkennung | 1 |
| **Endschalter / Mikroschalter** | Für genaue Positions-Erkennung | 2 |
| **Gehäuse IP65** | Wetterfester Anschlusskasten | 1 |

> 💡 **Tipp:** Ein 12 V → 5 V Step-Down-Modul (z. B. LM2596) spart ein zweites Netzteil, wenn der L298N keinen 5 V-Ausgang hat.

---

## 📍 Pin-Übersicht ESP32

| ESP32 GPIO | Funktion | Bauteil | Hinweis |
|---|---|---|---|
| **GPIO 25** | Motor IN1 | L298N IN1 | Richtungssteuerung |
| **GPIO 26** | Motor IN2 | L298N IN2 | Richtungssteuerung |
| **GPIO 27** | Motor ENA (PWM) | L298N ENA | Geschwindigkeit via LEDC |
| **GPIO 18** | Locklicht-Relais | Relais Kanal 1 | LOW = AN |
| **GPIO 19** | Stalllicht-Relais | Relais Kanal 2 | LOW = AN |
| **GPIO 33** | Taster Tür | Taster 1 | INPUT_PULLUP |
| **GPIO 32** | Taster Stalllicht | Taster 2 | INPUT_PULLUP |
| **GPIO 35** | Taster Rotlicht | Taster 3 | INPUT_PULLUP, **nur Eingang!** |
| **GPIO 34** | ACS712 analog | Stromsensor | INPUT only, kein PULLUP |
| **GPIO 21** | I²C SDA | VEML7700 + DS3231 | gemeinsamer Bus |
| **GPIO 22** | I²C SCL | VEML7700 + DS3231 | gemeinsamer Bus |
| **GPIO 14** | Endschalter AUF | Mikroschalter | LOW = Position erreicht |
| **GPIO 12** | Endschalter ZU | Mikroschalter | LOW = Position erreicht |
| **GPIO 4** | RGB Rot (PWM) | MOSFET Gate | via 470 Ω |
| **GPIO 16** | RGB Grün (PWM) | MOSFET Gate | via 470 Ω |
| **GPIO 17** | RGB Blau (PWM) | MOSFET Gate | via 470 Ω |

---

## 🔧 Schritt-für-Schritt Verkabelung

> ⚠️ **Sicherheit:** Vor jeder Verkabelungsarbeit die Stromversorgung trennen.  
> ⚠️ **Spannung:** 12 V und 3,3 V/5 V niemals vertauschen – der ESP32 verträgt maximal 3,3 V an den GPIO-Pins!

---

### 1 · Stromversorgung

```
12 V Netzteil (+) ──── L298N VSS (12 V Motoreingang)
12 V Netzteil (−) ──── L298N GND ──── Gemeinsame Masse (GND)

5 V Quelle (+) ──────── ESP32 VIN (oder USB)
5 V Quelle (−) ──────── ESP32 GND
```

> 💡 Wenn das L298N-Modul einen **5 V-Ausgang** hat (Jumper gesetzt und Motor ≤ 1 A), kann dieser direkt an den ESP32 VIN angeschlossen werden.

---

### 2 · Motor (L298N)

```
L298N IN1  ──── ESP32 GPIO 25
L298N IN2  ──── ESP32 GPIO 26
L298N ENA  ──── ESP32 GPIO 27
L298N OUT1 ──── Motor (+)
L298N OUT2 ──── Motor (−)
L298N GND  ──── Gemeinsame Masse
```

> ⚠️ Den **ENA-Jumper** auf dem L298N **entfernen**, sonst hat die PWM-Geschwindigkeitssteuerung keinen Effekt!

---

### 3 · I²C-Bus (VEML7700 + DS3231)

Beide Sensoren teilen sich denselben I²C-Bus:

```
ESP32 GPIO 21 (SDA) ──┬──── VEML7700 SDA
                      └──── DS3231 SDA

ESP32 GPIO 22 (SCL) ──┬──── VEML7700 SCL
                      └──── DS3231 SCL

ESP32 3,3 V ─────────┬──── VEML7700 VCC  ← unbedingt 3,3 V!
                     └──── DS3231 VCC    (3,3 V oder 5 V je nach Modul)

GND ─────────────────┬──── VEML7700 GND
                     └──── DS3231 GND
```

> ⚠️ Der VEML7700 ist ein **3,3 V-Sensor**. Niemals an 5 V anschließen!

---

### 4 · Relais (Locklicht & Stalllicht)

```
Relais IN1 ──── ESP32 GPIO 18    (Locklicht,  LOW = AN)
Relais IN2 ──── ESP32 GPIO 19    (Stalllicht, LOW = AN)
Relais VCC ──── 5 V
Relais GND ──── GND

Relais 1 COM ──── 12 V (+)
Relais 1 NO  ──── Locklicht (+)
Locklicht (−) ─── GND

Relais 2 COM ──── 12 V (+)
Relais 2 NO  ──── Stalllicht (+)
Stalllicht (−) ── GND
```

> NO = Normally Open (Kontakt ist bei ausgeschaltetem Relais offen)

---

### 5 · Taster

Jeder Taster verbindet den GPIO einfach mit GND. Interne Pull-Up-Widerstände sind in der Firmware aktiviert.

```
Taster 1 (Tür)        Pin 1 ──── ESP32 GPIO 33
                      Pin 2 ──── GND

Taster 2 (Stalllicht) Pin 1 ──── ESP32 GPIO 32
                      Pin 2 ──── GND

Taster 3 (Rotlicht)   Pin 1 ──── ESP32 GPIO 35
                      Pin 2 ──── GND
```

> ⚠️ **GPIO 35** ist ein **Input-Only-Pin** am ESP32 – kein OUTPUT möglich, als Tastereingang aber einwandfrei nutzbar.

---

### 6 · RGB LED-Strip über MOSFET (optional)

Der 12 V LED-Strip wird **nicht direkt** am ESP32 angeschlossen, sondern über N-Kanal MOSFETs (IRLZ44N) geschaltet:

```
ESP32 GPIO 4  ──[470 Ω]──── Gate  MOSFET Rot
ESP32 GPIO 16 ──[470 Ω]──── Gate  MOSFET Grün
ESP32 GPIO 17 ──[470 Ω]──── Gate  MOSFET Blau

MOSFET Drain ──── LED-Strip Kanal (R / G / B)
MOSFET Source ─── GND
LED-Strip V+ ──── 12 V Netzteil (+)
```

**Schaltbild eines Kanals:**

```
ESP32 GPIO ──[470 Ω]── Gate ┐
                            IRLZ44N
                       Drain ──── LED-Strip Kanal (12 V)
                       Source ─── GND
```

> ⚠️ Niemals einen 12 V LED-Strip direkt an den ESP32 GPIO anschließen – das zerstört den Mikrocontroller sofort!

---

### 7 · Endschalter (optional)

```
Endschalter AUF   Pin 1 ──── ESP32 GPIO 14
                  Pin 2 ──── GND

Endschalter ZU    Pin 1 ──── ESP32 GPIO 12
                  Pin 2 ──── GND
```

LOW-Signal = Position erreicht. Interne Pull-Ups sind aktiv.  
Im Web-Interface unter **Erweiterte Einstellungen** aktivieren.

---

### 8 · ACS712 Stromsensor (optional)

```
ACS712 VCC  ──── 5 V
ACS712 GND  ──── GND
ACS712 OUT  ──── ESP32 GPIO 34    (analog, Input-Only)
ACS712 IP+  ──── Motor (+) Leitung
ACS712 IP−  ──── Motor (−) Richtung Last
```

---

## 📊 Steckverbindungen im Überblick

```
┌─────────────────────────────────────┐
│            ESP32 DevKit             │
│                                     │
│  GPIO 25 ──────────── L298N IN1     │
│  GPIO 26 ──────────── L298N IN2     │
│  GPIO 27 ──────────── L298N ENA     │
│  GPIO 18 ──────────── Relais IN1    │
│  GPIO 19 ──────────── Relais IN2    │
│  GPIO 21 (SDA) ─┬──── VEML7700 SDA │
│                 └──── DS3231 SDA    │
│  GPIO 22 (SCL) ─┬──── VEML7700 SCL │
│                 └──── DS3231 SCL    │
│  GPIO 33 ──────────── Taster 1      │
│  GPIO 32 ──────────── Taster 2      │
│  GPIO 35 ──────────── Taster 3      │
│  GPIO 14 ──────────── Endschalter ↑ │
│  GPIO 12 ──────────── Endschalter ↓ │
│  GPIO 34 ──────────── ACS712 OUT    │
│  GPIO  4 ──[470Ω]──── MOSFET R Gate│
│  GPIO 16 ──[470Ω]──── MOSFET G Gate│
│  GPIO 17 ──[470Ω]──── MOSFET B Gate│
│  3,3 V ─────────┬──── VEML7700 VCC │
│                 └──── DS3231 VCC    │
│  GND ──────────────── alle GNDs     │
└─────────────────────────────────────┘
```
