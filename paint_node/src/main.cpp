#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <math.h>
#include "config.h"

// ── MPU6050 register addresses ────────────────────────────────────────────────
#define PWR_MGMT_1   0x6B
#define ACCEL_CONFIG 0x1C
#define GYRO_CONFIG  0x1B
#define ACCEL_XOUT_H 0x3B

// ── LPF smoothing (0 = no filter, 1 = never updates) ─────────────────────────
static const float ALPHA = 0.25f;

// ── globals ───────────────────────────────────────────────────────────────────
static float s_roll  = 0.0f;
static float s_pitch = 0.0f;

static WiFiClient tcp;
static char       json_buf[128];

static uint32_t last_read_ms = 0;
static uint32_t last_send_ms = 0;

// ── helpers ───────────────────────────────────────────────────────────────────

static void mpuWrite(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission(true);
}

static bool mpuInit() {
    // WHO_AM_I check using repeated-start (same as working mpu_driver.cpp)
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x75);
    if (Wire.endTransmission(false) != 0) return false;
    uint8_t got = Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)1, (uint8_t)true);
    if (got != 1) return false;
    uint8_t who = Wire.read();
    // Accept all known MPU60x0/MPU92xx WHO_AM_I responses
    if (who != 0x68 && who != 0x69 && who != 0x70 &&
        who != 0x71 && who != 0x72 && who != 0x73) return false;

    mpuWrite(PWR_MGMT_1,   0x00);           // wake (no PLL, keeps it simple)
    delay(10);
    mpuWrite(0x19,         0x07);           // sample rate div → 125 Hz
    mpuWrite(0x1A,         0x03);           // DLPF 44 Hz
    mpuWrite(GYRO_CONFIG,  0x08);           // ±500 °/s  → 65.5 LSB/(°/s)
    mpuWrite(ACCEL_CONFIG, 0x08);           // ±4 g      → 8192 LSB/g
    return true;
}

// Reads accel (m/s²) — ±4g → 8192 LSB/g, same config as mpu_driver.cpp
static bool mpuRead(float &ax, float &ay, float &az) {
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(ACCEL_XOUT_H);
    if (Wire.endTransmission(false) != 0) return false;
    uint8_t got = Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)6, (uint8_t)true);
    if (got != 6) return false;

    uint8_t buf[6];
    for (int i = 0; i < 6; i++) buf[i] = Wire.read();

    const float scale = 9.81f / 8192.0f;
    ax = (int16_t)((buf[0] << 8) | buf[1]) * scale;
    ay = (int16_t)((buf[2] << 8) | buf[3]) * scale;
    az = (int16_t)((buf[4] << 8) | buf[5]) * scale;
    return true;
}

static void tcpEnsureConnected() {
    if (tcp.connected()) return;
    tcp.stop();
    Serial.print("[TCP] Connecting...");
    while (!tcp.connect(SERVER_IP, SERVER_PORT)) {
        Serial.print('.');
        delay(1000);
    }
    Serial.println(" OK");
}

// ── Arduino entry points ──────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(300);

    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);   // 100 kHz — more reliable for MPU6050
    delay(200);

    Serial.print("[WiFi] Connecting...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print('.');
    }
    Serial.printf(" OK  IP=%s\n", WiFi.localIP().toString().c_str());

    while (!mpuInit()) {
        Serial.println("[MPU] Not found — check wiring");
        delay(1000);
    }
    Serial.println("[MPU] Ready");

    tcpEnsureConnected();

    last_read_ms = millis();
    last_send_ms = millis();
}

void loop() {
    const uint32_t now = millis();

    // ── Read MPU at 50 Hz ─────────────────────────────────────────────────────
    if (now - last_read_ms >= READ_MS) {
        last_read_ms = now;

        float ax, ay, az;
        if (mpuRead(ax, ay, az)) {
            // roll  = tilt left/right
            // pitch = tilt forward/back
            float roll  = atan2f(ay, az);
            float pitch = atan2f(-ax, sqrtf(ay * ay + az * az));

            // Low-pass filter to smooth out jitter
            s_roll  = ALPHA * roll  + (1.0f - ALPHA) * s_roll;
            s_pitch = ALPHA * pitch + (1.0f - ALPHA) * s_pitch;
        }
    }

    // ── Send at 20 Hz ─────────────────────────────────────────────────────────
    if (now - last_send_ms >= SEND_MS) {
        last_send_ms = now;
        tcpEnsureConnected();

        snprintf(json_buf, sizeof(json_buf),
                 "{\"node\":\"mpu\",\"roll\":%.4f,\"pitch\":%.4f}\n",
                 s_roll, s_pitch);

        tcp.print(json_buf);

        Serial.printf("[TX] roll=%.3f  pitch=%.3f\n", s_roll, s_pitch);
    }
}
