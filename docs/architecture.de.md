# 🧠 Firmware Architektur

Dieses Diagramm zeigt den grundlegenden Aufbau der Firmware des Chicken Coop Door Controllers.

```mermaid
flowchart TD

A[VEML7700 Lux Sensor] --> B[Sensor Validierung]

B --> C[Median Filter]
C --> D[Exponentielle Glättung]

D --> E[Lux Trend Berechnung]
E --> F[Sonnenuntergang Prognose]

F --> G{Schwellwert erreicht?}

G -->|Ja| H[Vorlicht aktivieren]
H --> I[Tür schließen]

G -->|Nein| J[Prognose Timer]

I --> K[Nacht Sperre]

K --> L[Tür geschlossen]

%% Sicherheitssysteme

B --> M[Sensor Health Monitoring]
M --> N[Fallback System]

N --> O[Zeitbasierte Steuerung]

%% Connectivity

L --> P[MQTT Status Publish]
P --> Q[Home Assistant]

%% Webinterface

R[Web Interface] --> S[Manuelle Steuerung]
S --> I
```

---

# Firmware Komponenten

## Sensor Layer

* VEML7700 Lux Sensor
* automatische Gain Anpassung
* Sensor Health Monitoring
* I²C Bus Recovery

---

## Signal Processing

Verarbeitung der Rohdaten:

* Median Filter
* exponentielle Glättung
* Lux Trend Berechnung

---

## Decision Engine

Entscheidet über Öffnen oder Schließen:

* Lux Schwellenwerte
* Sonnenuntergang Prognose
* Wolken Erkennung

---

## Actuation Layer

Steuert die Hardware:

* Vorlicht
* Türmotor
* Endschalter

---

## Safety Layer

Sicherheitsfunktionen:

* Nacht Sperre
* Sensor Fallback
* Zeitbasierte Steuerung
* Endschalter Schutz

---

## Connectivity Layer

Kommunikation mit externen Systemen:

* MQTT
* Webinterface
* OTA Updates
