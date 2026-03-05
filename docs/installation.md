# Installation Guide

This guide explains how to install and run the **ESP32 Chicken Coop Door Controller**.

---

# 1. Requirements

Hardware:

* ESP32 development board
* VEML7700 lux sensor
* motor driver
* door motor
* limit switches
* coop light relay

Software:

* VS Code
* PlatformIO extension
* Git

---

# 2. Clone the Repository

Clone the project from GitHub:

```bash
git clone https://github.com/Huehnerfluesterer/ESP-Huehnerklappe.git
```

Open the folder in **VS Code with PlatformIO**.

---

# 3. Create Configuration File

Sensitive data like WiFi credentials are stored in a local config file.

Copy the template:

```bash
cp include/config.example.h include/config.h
```

Edit the file and enter your WiFi credentials.

Example:

```cpp
#define WIFI_SSID "your_wifi"
#define WIFI_PASSWORD "your_password"
```

---

# 4. Build Firmware

Compile the firmware using PlatformIO.

In VS Code:

```
PlatformIO → Build
```

---

# 5. Upload Firmware

Connect your ESP32 via USB and upload the firmware.

```
PlatformIO → Upload
```

---

# 6. First Startup

After booting the ESP32:

1. The controller connects to WiFi
2. MQTT connection is established
3. The light sensor initializes
4. Automatic door control starts

---

# 7. Optional: Home Assistant Integration

You can integrate the controller via MQTT.

See:

🇬🇧 [MQTT Guide (English)](mqtt.md)  
🇩🇪 [MQTT (Deutsch)](mqtt.de.md)
