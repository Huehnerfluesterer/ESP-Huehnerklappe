# 🐔 Smart Chicken Coop Door Controller

[![ESP32](https://img.shields.io/badge/ESP32-Compatible-blue)]()
[![PlatformIO](https://img.shields.io/badge/PlatformIO-Project-orange)]()
[![MQTT](https://img.shields.io/badge/MQTT-Supported-green)]()
[![License](https://img.shields.io/badge/License-MIT-yellow)]()

An **ESP32-based intelligent chicken coop door controller** using ambient light measurements, predictive sunset detection and multiple safety fallbacks.

The system uses a **VEML7700 ambient light sensor** combined with filtering and trend detection to determine the optimal time to open and close the coop door.

It is designed for **real-world conditions**, including clouds, storms, snow reflection and sensor noise.

---

# 🌍 Languages

🇬🇧 English
🇩🇪 [Deutsch](README.de.md)

---

# ✨ Key Features

## 🌅 Lux-Based Automation

Automatic door control based on ambient light.

Features include:

* VEML7700 lux sensor
* automatic sensor gain switching
* lux median filtering
* exponential smoothing
* lux trend detection

Reliable detection of **sunrise and sunset conditions** even during cloudy weather.

---

## 📉 Predictive Sunset Detection

The firmware calculates the **lux change rate** to estimate when the closing threshold will be reached.

```
minutesToThresh = (lux - threshold) / luxRate
```

Advantages:

* earlier preparation for closing
* smoother dusk behaviour
* more accurate closing timing

---

## 💡 Pre-Close Coop Light

Before closing the door, the coop light turns on for a configurable duration.

Purpose:

* chickens gather inside the coop
* safer closing
* improved winter reliability

---

## 🌥 Cloud Stability Detection

Temporary brightness increases caused by clouds are detected and the closing prediction is cancelled:

```
lux > closeThreshold + PRECLOSE_ABORT_MARGIN_LX
```

This prevents premature closing.

---

# 🔄 Fallback Systems

The controller includes multiple fallback mechanisms.

## ⏰ Time-Based Fallback

If the lux sensor becomes unavailable, the system switches to **time-based door control**.

Triggers:

* sensor failure
* invalid lux values
* I²C errors
* sensor timeout

---

## 📡 Sensor Health Monitoring

The system continuously checks:

* lux validity
* I²C communication
* sensor response time
* auto-recovery attempts

---

## 🔌 I²C Bus Recovery

If the I²C bus locks up, the controller performs:

* I²C bus reset
* sensor reinitialization
* automatic reconnection

---

# 🛑 Safety Features

## Door Limit Switches

Physical limit switches detect:

* door fully open
* door fully closed

Prevents motor damage and mechanical stress.

---

## Night Lock

Once the door closes at night:

* reopening is blocked
* prevents false triggers caused by lights
* reset only occurs in the morning window

---

## Light Glitch Filtering

Short light spikes are filtered using:

* lux hysteresis
* hold timers
* trend filtering

Ignored events include:

* lightning
* car headlights
* camera flashes

---

# 📊 Light Processing Pipeline

The light measurement pipeline includes:

1. raw sensor reading
2. median filtering
3. exponential smoothing
4. trend calculation
5. prediction logic

This ensures **stable behavior in noisy environments**.

---

# 🌐 Connectivity

## MQTT Integration

Status and control via MQTT.

Examples:

* door state
* lux values
* sensor health
* manual commands

Compatible with:

* Home Assistant
* Node-RED
* other smart home systems

---

# 🖥 Web Interface

Built-in web interface for:

* configuration
* manual door control
* system status
* event logs

---

# 🔧 OTA Updates

Firmware updates can be installed **over the air** without physical access.

---

# 📜 Event Logging

The system logs:

* door movements
* lux conditions
* prediction triggers
* fallback activation
* sensor errors

---

# 🛠 Hardware

Typical setup:

* ESP32
* VEML7700 lux sensor
* motor driver
* door motor
* limit switches
* coop light

---

# 📷 Hardware Wiring

![Wiring](docs/images/wiring.png)

---

# 🧠 System Architecture

![Architecture](docs/images/architecture.png)

---

# 📜 License

MIT License

---

# 🤝 Contributions

Pull requests and improvements are welcome.
