#pragma once

#define WIFI_SSID   "kroussea"
#define WIFI_PASS   "krousseau1"
#define SERVER_IP   "172.20.10.11"
#define SERVER_PORT 9000

#define SDA_PIN 22
#define SCL_PIN 21

// MPU6050 address — AD0 → VCC → 0x69
#define MPU_ADDR 0x69

// Loop rates
#define READ_MS  20    // 50 Hz
#define SEND_MS  50    // 20 Hz
