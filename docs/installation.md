# Installation Guide

This guide explains how to install the **ESP32 Chicken Coop Door Controller**.

---

# 1. Requirements

Hardware:

* ESP32 development board
* VEML7700 lux sensor
* Motor driver
* Door motor
* Limit switches
* Relay for coop light

Software:

* VS Code
* PlatformIO extension
* Git

---

# 2. Clone Repository

Download the project from GitHub:

```bash
git clone https://github.com/Huehnerfluesterer/ESP-Huehnerklappe.git
```

Then open the project in **VS Code with PlatformIO**.

---

# 3. Create Configuration File

Credentials such as WiFi are stored in a local configuration file.

Copy the template:

```bash
cp include/config.example.h include/config.h
```

Then edit the file and insert your credentials.

Example:

```cpp
#define WIFI_SSID "MyWiFi"
#define WIFI_PASSWORD "MyPassword"
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

Connect the ESP32 via USB and upload the firmware.

```
PlatformIO → Upload
```

---

# 6. First Start

After starting the controller:

1. ESP32 connects to the configured WiFi network
2. Sensors and peripherals are initialized
3. Automatic door control becomes active

---

# Alternative: First Setup using Precompiled Firmware

If you flashed a **precompiled `.bin` firmware**, the WiFi network can be configured via the setup portal.

### Steps

1. Flash the firmware to the ESP32
2. Power on the device
3. Connect to the WiFi network:

```
Huehnerklappe-Setup
```

4. Open the configuration page in your browser:

```
http://192.168.4.1
```

5. Enter your WiFi credentials and save

The controller will reboot and connect to your network.

---

# 7. Optional Home Assistant Integration

Integration via MQTT is possible.

See:

🇬🇧 [MQTT Guide (English)](mqtt.md)
🇩🇪 [MQTT Guide (Deutsch)](mqtt.de.md)
