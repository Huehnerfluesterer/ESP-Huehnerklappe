#pragma once
// Hinweis: Diese Datei heißt bewusst wlan.h (nicht WiFi.h),
// um Namenskollisionen mit der Arduino-WiFi-Bibliothek auf
// Windows (case-insensitives Dateisystem) zu vermeiden.
#include <Arduino.h>
#include <WiFi.h>

// WLAN verbinden (non-blocking, Backoff-Watchdog)
void wifiConnectNonBlocking();
void wifiWatchdog();
