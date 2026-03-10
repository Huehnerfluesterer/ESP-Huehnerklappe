#pragma once

// ==================================================
// PINS – Hühnerklappe ESP32
// ==================================================

// Motor (L298N oder ähnlich)
#define MOTOR_IN1       25
#define MOTOR_IN2       26
#define MOTOR_ENA       27   // PWM via LEDC

// Relais
#define RELAIS_PIN          18   // Locklicht-Relais (LOW-aktiv)
#define STALLLIGHT_RELAY_PIN 19  // Stalllicht-Relais (LOW-aktiv)

// Taster
#define BUTTON_PIN      33   // Taster Klappe (INPUT_PULLUP)
#define STALL_BUTTON_PIN 32  // Taster Stalllicht (INPUT_PULLUP)
#define RED_BUTTON_PIN  35   // Taster Rotlicht (INPUT_PULLUP, input-only GPIO)

// Sensoren
#define LDR_PIN         35   // LDR analog (nicht aktiv genutzt)
#define ACS712_PIN      34   // Stromsensor analog

// I2C (VEML7700 + RTC DS3231)
#define I2C_SDA         21
#define I2C_SCL         22

// Endschalter
#define LIMIT_OPEN_PIN  14   // LOW = Öffnungsposition erreicht
#define LIMIT_CLOSE_PIN 12   // LOW = Schließposition erreicht

// RGB LED-Streifen (12V, über N-Kanal MOSFETs)
// Schaltung: GPIO ──[470Ω]── Gate IRLZ44N, Drain → LED-Kanal, Source → GND
#define RGB_PIN_R       4    // war: WS2812_PIN
#define RGB_PIN_G      16
#define RGB_PIN_B      17
#define RGB_FREQ     1000    // 1 kHz PWM
#define RGB_BITS        8    // 8-Bit (0–255)

// Relais-Logik (aktiv LOW)
#define RELAY_ON  LOW
#define RELAY_OFF HIGH
