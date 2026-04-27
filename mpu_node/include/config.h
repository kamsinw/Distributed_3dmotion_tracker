#pragma once

// ---------------------------------------------------------------
// WiFi credentials — edit before flashing
// ---------------------------------------------------------------
#define WIFI_SSID   "kroussea"
#define WIFI_PASS   "krousseau1"

// ---------------------------------------------------------------
// Server — set to your Mac's local IP (e.g. 192.168.1.100)
// ---------------------------------------------------------------
#define SERVER_IP   "172.20.10.11"
#define SERVER_PORT 9001

// ---------------------------------------------------------------
// MPU9250 x2 — shared I2C bus, different addresses via AD0 pin
//   Sensor A: AD0 → GND   → address 0x68
//   Sensor B: AD0 → 3.3V  → address 0x69
// ---------------------------------------------------------------
#define SCL_PIN     21
#define SDA_PIN     22

// INT (data-ready) pins — one per sensor.
// GPIO 34 and 35 are input-only on ESP32 (no internal pull-up needed
// because MPU9250 INT is push-pull active-high by default).
#define INT_PIN_A   35   // MPU-A interrupt → GPIO 35
#define INT_PIN_B   34   // MPU-B interrupt → GPIO 34 (unused if only one sensor)

// ---------------------------------------------------------------
// Timing
// ---------------------------------------------------------------
#define MPU_LOOP_MS  20   // 50 Hz sensor pipeline
#define SEND_LOOP_MS 50   // 20 Hz TCP send

// ---------------------------------------------------------------
// Uncomment to enable verbose Serial debug output
// ---------------------------------------------------------------
#define DEBUG_MODE
