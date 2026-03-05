# Installationsanleitung

Diese Anleitung erklärt die Installation des **ESP32 Hühnerklappen Controllers**.

---

# 1. Voraussetzungen

Hardware:

* ESP32 Entwicklungsboard
* VEML7700 Lux Sensor
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

1. ESP32 verbindet sich mit WLAN
2. MQTT Verbindung wird aufgebaut
3. Lux Sensor wird initialisiert
4. Automatische Türsteuerung startet

---

# 7. Optionale Home Assistant Integration

Integration über MQTT möglich.

Siehe:

🇬🇧 [MQTT Guide (English)](docs/mqtt.md)  
🇩🇪 [MQTT (Deutsch)](docs/mqtt.de.md)
