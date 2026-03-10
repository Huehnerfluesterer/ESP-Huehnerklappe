# 📦 Installationsanleitung

🇬🇧 [English](installation.md) &nbsp;|&nbsp; 🇩🇪 Deutsch

---

## Inhaltsverzeichnis

- [Voraussetzungen](#-voraussetzungen)
- [Schritt 1 – Repository klonen](#schritt-1--repository-klonen)
- [Schritt 2 – WLAN konfigurieren](#schritt-2--wlan-konfigurieren)
- [Schritt 3 – Projekt öffnen](#schritt-3--projekt-in-vs-code-öffnen)
- [Schritt 4 – Firmware flashen](#schritt-4--firmware-flashen)
- [Schritt 5 – IP-Adresse finden](#schritt-5--ip-adresse-herausfinden)
- [Schritt 6 – Ersteinrichtung](#schritt-6--ersteinrichtung-im-web-interface)
- [OTA-Updates](#-ota-firmware-update)
- [Fehlerbehebung](#-fehlerbehebung)

---

## ✅ Voraussetzungen

### Software

| Tool | Zweck | Download |
|---|---|---|
| **Visual Studio Code** | Code-Editor | [code.visualstudio.com](https://code.visualstudio.com/) |
| **PlatformIO Extension** | Build-System für ESP32 | In VS Code: Extensions → „PlatformIO IDE" |
| **CP2102 oder CH340 Treiber** | USB-Seriell-Treiber (je nach ESP32-Modul) | Siehe unten |

**USB-Treiber:**

- CP2102: [silabs.com/developers/usb-to-uart-bridge-vcp-drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
- CH340: [wch-ic.com/downloads/CH341SER_ZIP.html](https://www.wch-ic.com/downloads/CH341SER_ZIP.html)

> Welchen Treiber du brauchst, steht meistens auf dem ESP32-Modul aufgedruckt oder in der Produktbeschreibung.

### Hardware

Alle Bauteile verkabelt gemäß [Hardware-Anleitung](HW.de.md).

---

## Schritt 1 – Repository klonen

```bash
git clone https://github.com/Huehnerfluesterer/ESP-Huehnerklappe.git
cd ESP-Huehnerklappe
```

Oder als ZIP herunterladen (GitHub → **Code → Download ZIP**) und entpacken.

---

## Schritt 2 – WLAN konfigurieren

Öffne die Datei `src/config.h` und trage deine WLAN-Zugangsdaten ein:

```cpp
#define WIFI_SSID     "Dein_WLAN_Name"
#define WIFI_PASSWORD "Dein_WLAN_Passwort"
```

> ⚠️ **Wichtig – Sicherheit:** Diese Datei enthält geheime Zugangsdaten.  
> Trage sie in `.gitignore` ein, **bevor** du das Projekt auf GitHub hochlädst:
>
> ```
> # .gitignore
> src/config.h
> ```

---

## Schritt 3 – Projekt in VS Code öffnen

1. VS Code starten
2. **Datei → Ordner öffnen** → Den Projektordner `ESP-Huehnerklappe` wählen
3. PlatformIO erkennt das Projekt automatisch anhand der `platformio.ini`
4. Beim ersten Öffnen werden alle Bibliotheken automatisch heruntergeladen:

| Bibliothek | Zweck |
|---|---|
| `Adafruit VEML7700` | Lichtsensor |
| `RTClib` | Echtzeituhr DS3231 |
| `ArduinoJson` | JSON für Web & MQTT |
| `PubSubClient` | MQTT-Client |

---

## Schritt 4 – Firmware flashen

1. ESP32 per USB mit dem Computer verbinden
2. Überprüfen, ob der COM-Port erkannt wird:
   - Windows: Geräte-Manager → Anschlüsse (COM & LPT)
   - macOS/Linux: `ls /dev/tty*`
3. In VS Code auf das **→ Pfeil-Symbol** unten klicken (**PlatformIO: Upload**)
4. Warten bis im Terminal erscheint:

```
Writing at 0x00008000... (100 %)
Wrote 123456 bytes
Hash of data verified.

[SUCCESS] Took 12.34 seconds
```

**Serielle Ausgabe beobachten** (empfohlen beim ersten Start):

Klick auf das **Stecker-Symbol** unten (PlatformIO: Serial Monitor). Folgendes sollte erscheinen:

```
🐔 Hühnerklappe – FW 1.0.15
✅ RTC per NTP synchronisiert
🚀 Hühnerklappe gestartet – FW 1.0.15
```

> Baudrate: **115 200**

---

## Schritt 5 – IP-Adresse herausfinden

Die IP-Adresse erscheint im seriellen Monitor nach dem WLAN-Connect. Alternativ:

- **Router-Oberfläche:** Unter verbundenen Geräten nach `Huehnerklappe-ESP32` suchen
- **Netzwerk-Scanner:** z. B. [Angry IP Scanner](https://angryip.org/) oder `nmap -sn 192.168.1.0/24`

Anschließend im Browser öffnen:

```
http://192.168.x.x
```

---

## Schritt 6 – Ersteinrichtung im Web-Interface

### 6.1 – Systemtest

Unter **`/systemtest`** prüfen, ob alle Komponenten erkannt werden:

| Komponente | Erwartetes Ergebnis |
|---|---|
| WLAN | ✅ Verbunden, RSSI angezeigt |
| VEML7700 | ✅ Lux-Wert > 0 |
| RTC DS3231 | ✅ Korrekte Uhrzeit |
| MQTT | ✅ (falls konfiguriert) |

---

### 6.2 – Motorpositionen einlernen (Lern-Modus)

Beim ersten Start müssen die Motorlaufzeiten eingelernt werden:

1. Web-Interface öffnen → **`/learn`**
2. **Phase 1:** Auf „Start" klicken → Motor fährt Tür vollständig auf → „Position bestätigen" klicken
3. **Phase 2:** Motor fährt Tür vollständig zu → „Position bestätigen" klicken
4. Positionen werden im EEPROM dauerhaft gespeichert

> Nach einem Neustart werden die eingelernte Positionen automatisch geladen.

---

### 6.3 – Betriebsmodus wählen

Unter **`/settings`**:

| Modus | Beschreibung |
|---|---|
| `lux` | Automatisch anhand Helligkeit (empfohlen) |
| `time` | Automatisch anhand fester Uhrzeiten |
| `manual` | Nur manuell über Taster / Web / MQTT |

---

### 6.4 – Lux-Schwellwerte anpassen

Die Standardwerte sind ein guter Ausgangspunkt. Für deinen Standort optimieren:

1. Systemtest öffnen → aktuellen Lux-Wert ablesen
2. An einem typischen Morgen / Abend messen, bei welchem Lux-Wert du die Tür öffnen/schließen möchtest
3. Werte in den Einstellungen speichern

**Richtwerte:**

| Situation | Lux |
|---|---|
| Klare Nacht | < 1 lx |
| Tiefe Dämmerung | 1 – 10 lx |
| Schließen empfohlen | 10 – 50 lx |
| Öffnen empfohlen | 100 – 500 lx |
| Bewölkter Tag | 1 000 – 10 000 lx |
| Sonniger Tag | > 10 000 lx |

---

### 6.5 – MQTT konfigurieren (optional)

Unter **`/mqtt`** eintragen:

| Feld | Beispiel |
|---|---|
| Host | `192.168.1.100` |
| Port | `1883` |
| Client-ID | `huehnerklappe` |
| Base-Topic | `huehnerklappe` |
| Benutzer/Passwort | optional |

Mit **„Verbindung testen"** prüfen, ob die Verbindung funktioniert.

---

## ✅ Erstinbetriebnahme-Checkliste

- [ ] Hardware verkabelt (alle Verbindungen geprüft)
- [ ] `src/config.h` mit WLAN-Zugangsdaten befüllt
- [ ] `src/config.h` in `.gitignore` eingetragen
- [ ] Firmware geflasht, serieller Monitor zeigt `🚀 Hühnerklappe gestartet`
- [ ] IP-Adresse notiert, Web-Interface erreichbar
- [ ] Systemtest unter `/systemtest` durchgeführt (alle ✅)
- [ ] Lern-Modus abgeschlossen (Motorpositionen gespeichert)
- [ ] Betriebsmodus gewählt und gespeichert
- [ ] Lux-Schwellwerte an Standort angepasst
- [ ] Testlauf: Tür manuell öffnen und schließen
- [ ] Optional: MQTT konfiguriert und getestet

---

## 🔧 OTA Firmware-Update

Nach der Erstinstallation können Updates **kabellos** eingespielt werden:

1. Neue Firmware-Datei (`.bin`) bereitstellen:
   - In VS Code: **PlatformIO → Build** → Datei unter `.pio/build/esp32dev/firmware.bin`
2. Web-Interface öffnen → **`/fw`**
3. Datei auswählen und hochladen
4. Der ESP32 startet nach dem Update automatisch neu

> ⚠️ Während des OTA-Updates wird der Motor automatisch gestoppt und alle Ausgänge werden abgeschaltet.

---

## 🔍 Fehlerbehebung

### ESP32 wird nicht als COM-Port erkannt

→ USB-Treiber installieren (CP2102 oder CH340, siehe Voraussetzungen).  
→ Anderes USB-Kabel probieren (manche Kabel sind nur Ladekabel ohne Datenleitungen).

### Upload schlägt fehl: „Connecting...____"

→ `BOOT`-Taste auf dem ESP32 gedrückt halten, während der Upload startet – dann loslassen.  
→ Upload-Geschwindigkeit in `platformio.ini` auf `460800` reduzieren.

### VEML7700 nicht gefunden im Systemtest

→ I²C-Verbindung prüfen: SDA = GPIO 21, SCL = GPIO 22.  
→ Spannung am VEML7700 prüfen: muss **3,3 V** sein, nicht 5 V.  
→ I²C-Adresse prüfen: VEML7700 hat Adresse `0x10`.

### RTC DS3231 nicht gefunden

→ I²C-Bus prüfen (gemeinsamer Bus mit VEML7700).  
→ CR2032-Akku im DS3231-Modul eingesetzt?

### Motor dreht nicht

→ ENA-Jumper am L298N entfernt?  
→ L298N mit 12 V versorgt?  
→ GPIO 27 korrekt verbunden (PWM für ENA)?

### Tür öffnet/schließt nicht automatisch

→ Lux-Schwellwerte prüfen – ggf. anpassen (Systemtest zeigt aktuellen Lux-Wert).  
→ Betriebsmodus in Einstellungen auf `lux` gesetzt?

### Build-Fehler auf Windows: `WiFi.h not found`

→ Prüfen, ob eine Datei `src/WiFi.cpp` im Projektordner liegt – diese löschen.  
Windows unterscheidet keine Groß-/Kleinschreibung, wodurch ein Namenskonflikt mit dem Framework-Header `<WiFi.h>` entsteht.
