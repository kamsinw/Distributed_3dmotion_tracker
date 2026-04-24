// MPU6050 driver — raw Wire, no third-party library.
// Uses Wire.endTransmission(false) (repeated-start) for register reads,
// which is required for reliable I2C on ESP32.

#include "mpu_driver.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>

// ── MPU6050 register map ──────────────────────────────────────────────
static constexpr uint8_t REG_SMPLRT_DIV   = 0x19;
static constexpr uint8_t REG_CONFIG       = 0x1A;
static constexpr uint8_t REG_GYRO_CONFIG  = 0x1B;
static constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
static constexpr uint8_t REG_INT_ENABLE   = 0x38;
static constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;  // 14 bytes: ax ay az temp gx gy gz
static constexpr uint8_t REG_PWR_MGMT_1   = 0x6B;
static constexpr uint8_t REG_WHO_AM_I     = 0x75;

// ── Physical addresses ────────────────────────────────────────────────
static constexpr uint8_t ADDR_A = 0x69;  // AD0 → VCC
static constexpr uint8_t ADDR_B = 0x68;  // AD0 → GND

static bool active_a = false;
static bool active_b = false;

// Data-ready ISR flags
static volatile bool data_ready_a = false;
static volatile bool data_ready_b = false;

static void IRAM_ATTR mpuISR_A() { data_ready_a = true; }
static void IRAM_ATTR mpuISR_B() { data_ready_b = true; }

// Per-sensor accelerometer bias
static float bias_ax_a = 0, bias_ay_a = 0, bias_az_a = 0;
static float bias_ax_b = 0, bias_ay_b = 0, bias_az_b = 0;

// LPF state
static float filt_ax = 0, filt_ay = 0, filt_az = 0;
static float filt_gx = 0, filt_gy = 0, filt_gz = 0;
static constexpr float ALPHA = 0.7f;

// ±4 g  → LSB/g = 8192  →  m/s² = raw / 8192 * 9.81
// ±500 °/s → LSB/(°/s) = 65.5
static constexpr float ACCEL_SCALE = 9.81f / 8192.0f;
static constexpr float GYRO_SCALE  = 1.0f  / 65.5f;

// ── Raw I2C helpers ───────────────────────────────────────────────────

static bool writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

// Repeated-start read: write register address then read bytes without
// releasing the bus (sendStop=false keeps SCL low between phases).
static bool readRegs(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;  // repeated start
    uint8_t got = Wire.requestFrom((uint8_t)addr, len, (uint8_t)true);
    if (got != len) return false;
    for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
}

static bool checkPresent(uint8_t addr) {
    uint8_t who = 0;
    if (!readRegs(addr, REG_WHO_AM_I, &who, 1)) return false;
    // WHO_AM_I bits [6:1] = 0x34 → full byte = 0x68 for all MPU6050 variants
    return (who == 0x68 || who == 0x72 || who == 0x70);
}

static bool initSensor(uint8_t addr) {
    if (!writeReg(addr, REG_PWR_MGMT_1, 0x00)) return false;  // wake up
    delay(10);
    writeReg(addr, REG_SMPLRT_DIV,   0x07);   // 1 kHz / (1+7) = 125 Hz
    writeReg(addr, REG_CONFIG,        0x03);   // DLPF ~44 Hz
    writeReg(addr, REG_GYRO_CONFIG,   0x08);   // ±500 °/s
    writeReg(addr, REG_ACCEL_CONFIG,  0x08);   // ±4 g
    writeReg(addr, REG_INT_ENABLE,    0x01);   // data-ready interrupt
    return true;
}

static bool readRaw(uint8_t addr,
                    float &ax, float &ay, float &az,
                    float &gx, float &gy, float &gz) {
    uint8_t buf[14];
    if (!readRegs(addr, REG_ACCEL_XOUT_H, buf, 14)) return false;

    int16_t raw_ax = (int16_t)((buf[0]  << 8) | buf[1]);
    int16_t raw_ay = (int16_t)((buf[2]  << 8) | buf[3]);
    int16_t raw_az = (int16_t)((buf[4]  << 8) | buf[5]);
    // buf[6..7] = temperature, skip
    int16_t raw_gx = (int16_t)((buf[8]  << 8) | buf[9]);
    int16_t raw_gy = (int16_t)((buf[10] << 8) | buf[11]);
    int16_t raw_gz = (int16_t)((buf[12] << 8) | buf[13]);

    ax = raw_ax * ACCEL_SCALE;
    ay = raw_ay * ACCEL_SCALE;
    az = raw_az * ACCEL_SCALE;
    gx = raw_gx * GYRO_SCALE;
    gy = raw_gy * GYRO_SCALE;
    gz = raw_gz * GYRO_SCALE;
    return true;
}

// ── Public API ────────────────────────────────────────────────────────

bool mpuInit() {
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);
    delay(200);  // MPU6050 power-on reset takes up to 100 ms

    active_a = checkPresent(ADDR_A) && initSensor(ADDR_A);
    active_b = checkPresent(ADDR_B) && initSensor(ADDR_B);

    if (active_a) {
        pinMode(INT_PIN_A, INPUT);
        attachInterrupt(digitalPinToInterrupt(INT_PIN_A), mpuISR_A, RISING);
    }
    if (active_b) {
        pinMode(INT_PIN_B, INPUT);
        attachInterrupt(digitalPinToInterrupt(INT_PIN_B), mpuISR_B, RISING);
    }

#ifdef DEBUG_MODE
    Serial.printf("[MPU] Sensor A (0x69 AD0→VCC): %s  INT→GPIO%d\n",
                  active_a ? "OK" : "NOT FOUND", INT_PIN_A);
    Serial.printf("[MPU] Sensor B (0x68 AD0→GND): %s  INT→GPIO%d\n",
                  active_b ? "OK" : "NOT FOUND", INT_PIN_B);
#endif

    return (active_a || active_b);
}

uint8_t mpuActiveMask() {
    return (active_a ? 0x01 : 0x00) | (active_b ? 0x02 : 0x00);
}

void mpuCalibrate(int samples) {
    // Use timed polling — does not rely on INT pin being wired.
    auto calibrateSensor = [&](uint8_t addr, bool active,
                               float &bax, float &bay, float &baz,
                               const char* label) {
        if (!active) return;
        double sx = 0, sy = 0, sz = 0;
        int n = 0;
        while (n < samples) {
            float ax, ay, az, gx, gy, gz;
            if (readRaw(addr, ax, ay, az, gx, gy, gz)) {
                sx += ax; sy += ay; sz += az;
                n++;
            }
            delay(8);  // ~125 Hz
        }
        bax = (float)(sx / samples);
        bay = (float)(sy / samples);
        baz = (float)(sz / samples) - 9.81f;
#ifdef DEBUG_MODE
        Serial.printf("[CAL-%s] bias ax=%.4f ay=%.4f az=%.4f\n",
                      label, bax, bay, baz);
#endif
    };

    calibrateSensor(ADDR_A, active_a, bias_ax_a, bias_ay_a, bias_az_a, "A");
    calibrateSensor(ADDR_B, active_b, bias_ax_b, bias_ay_b, bias_az_b, "B");
}

bool mpuRead(RawImuData &out) {
    float raw_ax = 0, raw_ay = 0, raw_az = 0;
    float raw_gx = 0, raw_gy = 0, raw_gz = 0;
    int contributors = 0;

    auto tryRead = [&](uint8_t addr, bool active,
                       float bax, float bay, float baz) {
        if (!active) return;
        float ax, ay, az, gx, gy, gz;
        if (readRaw(addr, ax, ay, az, gx, gy, gz)) {
            raw_ax += ax - bax;
            raw_ay += ay - bay;
            raw_az += az - baz;
            raw_gx += gx;
            raw_gy += gy;
            raw_gz += gz;
            contributors++;
        }
    };

    tryRead(ADDR_A, active_a, bias_ax_a, bias_ay_a, bias_az_a);
    tryRead(ADDR_B, active_b, bias_ax_b, bias_ay_b, bias_az_b);

    if (contributors == 0) return false;

    const float inv = 1.0f / (float)contributors;
    raw_ax *= inv; raw_ay *= inv; raw_az *= inv;
    raw_gx *= inv; raw_gy *= inv; raw_gz *= inv;

    filt_ax = ALPHA * filt_ax + (1.0f - ALPHA) * raw_ax;
    filt_ay = ALPHA * filt_ay + (1.0f - ALPHA) * raw_ay;
    filt_az = ALPHA * filt_az + (1.0f - ALPHA) * raw_az;
    filt_gx = ALPHA * filt_gx + (1.0f - ALPHA) * raw_gx;
    filt_gy = ALPHA * filt_gy + (1.0f - ALPHA) * raw_gy;
    filt_gz = ALPHA * filt_gz + (1.0f - ALPHA) * raw_gz;

    out.ax = filt_ax; out.ay = filt_ay; out.az = filt_az;
    out.gx = filt_gx; out.gy = filt_gy; out.gz = filt_gz;

#ifdef DEBUG_MODE
    Serial.printf("[MPU] ax=%.3f ay=%.3f az=%.3f  gx=%.3f gy=%.3f gz=%.3f\n",
                  out.ax, out.ay, out.az, out.gx, out.gy, out.gz);
#endif

    return true;
}
