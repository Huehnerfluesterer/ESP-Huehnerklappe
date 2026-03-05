# Installationsanleitung

Diese Anleitung erklärt die Installation des **ESP32 Hühnerklappen Controllers**.

---

# 1. Voraussetzungen

Hardware:

* ESP32 Entwicklungsboard
* VEML7700 Lux-Sensor
* Motortreiber
* Türmotor
* Endschalter
* Relais für Stalllicht

Software:

* VS Code
* PlatformIO Erweiterung
* Git

---

# 2. Repository klonen

Projekt von GitHub herunterladen:

```bash
git clone https://github.com/Huehnerfluesterer/ESP-Huehnerklappe.git
```

Danach das Projekt in **VS Code mit PlatformIO öffnen**.

---

# 3. Konfigurationsdatei erstellen

Zugangsdaten wie WLAN werden in einer lokalen Konfigurationsdatei gespeichert.

Vorlage kopieren:

```bash
cp include/config.example.h include/config.h
```

Danach die Datei bearbeiten und eigene Daten eintragen.

Beispiel:

```cpp
#define WIFI_SSID "MeinWLAN"
#define WIFI_PASSWORD "MeinPasswort"
```

---

# 4. Firmware kompilieren

Firmware mit PlatformIO bauen.

In VS Code:

```
PlatformIO → Build
```

---

# 5. Firmware hochladen

ESP32 per USB verbinden und Firmware hochladen.

```
PlatformIO → Upload
```

---

# 6. Erster Start

Nach dem Start des Controllers:

1. Der ESP32 verbindet sich mit dem konfigurierten WLAN
2. Sensoren und Peripherie werden initialisiert
3. Die automatische Türsteuerung wird aktiviert

---

# Alternative: Ersteinrichtung mit vorgefertigter Firmware (.bin)

Wenn eine **vorkompilierte `.bin` Firmware** geflasht wurde, kann das WLAN über das Setup-Portal eingerichtet werden.

### Schritte

1. Firmware auf den ESP32 flashen
2. Gerät einschalten
3. Mit folgendem WLAN verbinden:

```
Huehnerklappe-Setup
```

4. Konfigurationsseite im Browser öffnen:

```
http://192.168.4.1
```

5. WLAN-Daten eingeben und speichern

Der Controller startet anschließend neu und verbindet sich automatisch mit dem Netzwerk.

---

# 7. Optionale Home Assistant Integration

Eine Integration über MQTT ist möglich.

Siehe:

🇬🇧 [MQTT Guide (English)](mqtt.md)
🇩🇪 [MQTT Anleitung (Deutsch)](mqtt.de.md)
