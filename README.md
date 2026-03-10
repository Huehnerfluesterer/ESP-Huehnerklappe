<p align="center">
  <img src="docs/images/esp-huehnerklappe.png" width="900">
</p>

<h1 align="center">🐔 ESP32 Chicken Coop Door Controller</h1>
<p align="center">
  Lux-based automation &nbsp;•&nbsp; Sunset prediction &nbsp;•&nbsp; MQTT integration &nbsp;•&nbsp; Web interface &nbsp;•&nbsp; OTA updates
</p>

<p align="center">
  <img src="https://img.shields.io/badge/ESP32-Compatible-blue" />
  <img src="https://img.shields.io/badge/PlatformIO-Project-orange" />
  <img src="https://img.shields.io/badge/Firmware-v1.0.15-brightgreen" />
  <img src="https://img.shields.io/badge/MQTT-Supported-green" />
  <img src="https://img.shields.io/badge/License-MIT-yellow" />
</p>

---

An **ESP32-based intelligent chicken coop door controller** using ambient light measurements, predictive sunset detection, and multiple safety fallbacks.

The system uses a **VEML7700 ambient light sensor** combined with median filtering, exponential smoothing, and trend detection to determine the optimal time to open and close the coop door. It is designed for **real-world conditions** including clouds, rain, snow reflection, and sensor noise.

---

## 🌍 Languages

🇬🇧 English  
🇩🇪 [Deutsch](README.de.md)

---

## ✨ Key Features

| Feature | Description |
|---|---|
| 🌅 **Lux Automation** | Opens/closes based on ambient light via VEML7700 |
| 📉 **Sunset Prediction** | Predicts closing time from lux change rate |
| 💡 **Pre-Close Light** | Coop light turns on before closing to gather chickens |
| 🌥 **Cloud Detection** | Cancels false closing triggers caused by passing clouds |
| ⏰ **Time Fallback** | Switches to time-based control if sensor fails |
| 🛑 **Limit Switches** | Optional hardware end-stop detection |
| 📡 **MQTT** | Full Home Assistant / Node-RED integration |
| 🌐 **Web Interface** | Browser-based control and configuration |
| 🔧 **OTA Updates** | Firmware updates over the air |
| 📜 **Event Log** | Persistent log stored in EEPROM |
| 🔴 **RGB Light** | PWM-controlled RGB LED strip via N-channel MOSFETs |

---

## 🌅 Lux-Based Automation

Automatic door control based on ambient light using the VEML7700 sensor.

The measurement pipeline processes each reading through multiple stages:

1. Raw sensor reading with auto gain switching
2. Median filter (buffer size 5) to suppress noise spikes
3. Exponential smoothing (EMA) for stable values
4. Lux trend calculation (rate of change over 30 s)
5. Predictive closing logic

Reliable detection of **sunrise and sunset** even during cloudy weather.

---

## 📉 Predictive Sunset Detection

The firmware calculates the **lux change rate** to estimate when the closing threshold will be reached:

```
minutesToThresh = (lux - threshold) / luxRate
```

This allows the system to prepare for closing **before** the threshold is reached, resulting in smoother dusk behaviour and more accurate closing timing.

---

## 💡 Pre-Close Coop Light

Before closing, the coop light turns on for a configurable duration so that chickens gather inside. If lux rises again (cloud passing), the pre-close sequence is **automatically cancelled**.

---

## 🌥 Cloud Stability Detection

Temporary brightness increases are detected and the closing prediction is cancelled:

```
lux > closeThreshold + PRECLOSE_ABORT_MARGIN_LX
```

Prevents premature closing caused by passing clouds.

---

## 🔄 Fallback Systems

### ⏰ Time-Based Fallback

If the lux sensor becomes unavailable, the system switches to **time-based door control** automatically. Triggers:

- Sensor hardware failure
- Invalid lux values
- I²C communication error
- Sensor response timeout

### 📡 Sensor Health Monitoring

Continuously checks lux validity, I²C communication, sensor response times, and triggers auto-recovery.

### 🔌 I²C Bus Recovery

If the I²C bus locks up: bus reset via 9 clock pulses + STOP condition, followed by sensor reinitialization and automatic reconnection.

---

## 🛑 Safety Features

**Door Limit Switches** — Physical end-stops detect door fully open / fully closed. Prevents motor damage and mechanical stress. Can be enabled/disabled in the web interface.

**Night Lock** — Once the door closes at night, reopening is blocked until the morning window. Prevents false triggers from outdoor lights.

**Light Glitch Filtering** — Short spikes from lightning, car headlights, or camera flashes are suppressed via lux hysteresis and hold timers.

**Motor Blockage Detection** — ACS712 current sensor detects stall conditions. The motor reverses briefly after a blockage.

---

## 🌐 Connectivity

### MQTT Integration

Full status and control via MQTT, compatible with Home Assistant, Node-RED, and other smart home systems. See [MQTT documentation](docs/mqtt.de.md).

### Web Interface

Built-in web server on port 80. Accessible from any browser on the local network. Installable as a **PWA** on mobile devices.

| Route | Function |
|---|---|
| `/` | Dashboard: status, door control |
| `/settings` | Automation settings |
| `/mqtt` | MQTT configuration |
| `/learn` | Motor position learning |
| `/log` | Event log |
| `/systemtest` | Hardware self-test |
| `/fw` | OTA firmware update |

---

## 🛠 Hardware

Typical setup:

- ESP32 DevKit v1
- VEML7700 lux sensor (I²C)
- DS3231 RTC module (I²C)
- L298N motor driver
- 12 V DC door motor
- 2× relay module (coop light, stable light)
- 3× push buttons
- Optional: limit switches, ACS712 current sensor, RGB LED strip with IRLZ44N MOSFETs

See the full wiring guide: [Hardware & Wiring (Deutsch)](docs/HW.de.md)

---

## 📦 Installation

See the step-by-step installation guide:

🇬🇧 [Installation Guide (English)](docs/installation.md)  
🇩🇪 [Installationsanleitung (Deutsch)](docs/installation.de.md)

---

## 📷 Hardware Wiring

![Wiring](docs/images/wiring.png)

---

## 🧠 System Architecture

![Architecture](docs/images/architecture.png)

---

## 📜 License

MIT License — free to use, modify, and distribute.

---

## 🤝 Contributions

Pull requests and improvements are welcome.
