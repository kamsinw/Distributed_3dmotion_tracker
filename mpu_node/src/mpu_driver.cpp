// MPU driver — raw Wire, single sensor (Sensor A, 0x69, AD0→VCC).
// Uses Wire.endTransmission(false) for reliable repeated-start reads.

#include "mpu_driver.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>

// ── Register map ──────────────────────────────────────────────────────
static constexpr uint8_t REG_SMPLRT_DIV   = 0x19;
static constexpr uint8_t REG_CONFIG       = 0x1A;
static constexpr uint8_t REG_GYRO_CONFIG  = 0x1B;
static constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;
static constexpr uint8_t REG_INT_ENABLE   = 0x38;
static constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;
static constexpr uint8_t REG_PWR_MGMT_1   = 0x6B;
static constexpr uint8_t REG_WHO_AM_I     = 0x75;

// Single active sensor — AD0 → VCC → address 0x69
static constexpr uint8_t ADDR_A = 0x69;
static bool active_a = false;

// Accelerometer bias from calibration
static float bias_ax = 0, bias_ay = 0, bias_az = 0;

// LPF — fixed at 0.3 (no adaptive switching to avoid discontinuities)
static float filt_ax = 0, filt_ay = 0, filt_az = 0;
static float filt_gx = 0, filt_gy = 0, filt_gz = 0;
static float ALPHA = 0.3f;

// ±4 g → 8192 LSB/g → m/s²   |   ±500°/s → 65.5 LSB/(°/s)
static constexpr float ACCEL_SCALE = 9.81f / 8192.0f;
static constexpr float GYRO_SCALE  = 1.0f  / 65.5f;

// ── I2C helpers ───────────────────────────────────────────────────────

static bool writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

static bool readRegs(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    uint8_t got = Wire.requestFrom((uint8_t)addr, len, (uint8_t)true);
    if (got != len) return false;
    for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
}

static bool checkPresent(uint8_t addr) {
    uint8_t who = 0;
    if (!readRegs(addr, REG_WHO_AM_I, &who, 1)) return false;
    return (who == 0x68 || who == 0x70 || who == 0x71 ||
            who == 0x72 || who == 0x73);
}

static bool initSensor(uint8_t addr) {
    if (!writeReg(addr, REG_PWR_MGMT_1, 0x00)) return false;
    delay(10);
    writeReg(addr, REG_SMPLRT_DIV,   0x07);  // 125 Hz
    writeReg(addr, REG_CONFIG,        0x03);  // DLPF 44 Hz
    writeReg(addr, REG_GYRO_CONFIG,   0x08);  // ±500°/s
    writeReg(addr, REG_ACCEL_CONFIG,  0x08);  // ±4 g
    writeReg(addr, REG_INT_ENABLE,    0x01);  // data-ready interrupt
    return true;
}

static bool readRaw(float &ax, float &ay, float &az,
                    float &gx, float &gy, float &gz) {
    uint8_t buf[14];
    if (!readRegs(ADDR_A, REG_ACCEL_XOUT_H, buf, 14)) return false;
    ax = (int16_t)((buf[0]  << 8) | buf[1])  * ACCEL_SCALE;
    ay = (int16_t)((buf[2]  << 8) | buf[3])  * ACCEL_SCALE;
    az = (int16_t)((buf[4]  << 8) | buf[5])  * ACCEL_SCALE;
    gx = (int16_t)((buf[8]  << 8) | buf[9])  * GYRO_SCALE;
    gy = (int16_t)((buf[10] << 8) | buf[11]) * GYRO_SCALE;
    gz = (int16_t)((buf[12] << 8) | buf[13]) * GYRO_SCALE;
    return true;
}

// ── Public API ────────────────────────────────────────────────────────

void mpuSetAlpha(float alpha) { ALPHA = alpha; }

bool mpuInit() {
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(100000);
    delay(200);

    active_a = checkPresent(ADDR_A) && initSensor(ADDR_A);

    if (active_a) {
        pinMode(INT_PIN_A, INPUT);
        attachInterrupt(digitalPinToInterrupt(INT_PIN_A),
                        []() IRAM_ATTR {}, RISING);
    }

#ifdef DEBUG_MODE
    Serial.printf("[MPU] Sensor A (0x69 AD0→VCC): %s\n",
                  active_a ? "OK" : "NOT FOUND");
#endif
    return active_a;
}

uint8_t mpuActiveMask() { return active_a ? 0x01 : 0x00; }

void mpuCalibrate(int samples) {
    if (!active_a) return;
    double sx = 0, sy = 0, sz = 0;
    int n = 0;
    while (n < samples) {
        float ax, ay, az, gx, gy, gz;
        if (readRaw(ax, ay, az, gx, gy, gz)) {
            sx += ax; sy += ay; sz += az;
            n++;
        }
        delay(8);
    }
    bias_ax = (float)(sx / samples);
    bias_ay = (float)(sy / samples);
    bias_az = (float)(sz / samples) - 9.81f;
#ifdef DEBUG_MODE
    Serial.printf("[CAL] bias ax=%.4f ay=%.4f az=%.4f\n",
                  bias_ax, bias_ay, bias_az);
#endif
}

bool mpuRead(RawImuData &out) {
    if (!active_a) return false;

    float ax, ay, az, gx, gy, gz;
    if (!readRaw(ax, ay, az, gx, gy, gz)) return false;

    float raw_ax = ax - bias_ax;
    float raw_ay = ay - bias_ay;
    float raw_az = az - bias_az;

    filt_ax = ALPHA*filt_ax + (1.0f-ALPHA)*raw_ax;
    filt_ay = ALPHA*filt_ay + (1.0f-ALPHA)*raw_ay;
    filt_az = ALPHA*filt_az + (1.0f-ALPHA)*raw_az;
    filt_gx = ALPHA*filt_gx + (1.0f-ALPHA)*gx;
    filt_gy = ALPHA*filt_gy + (1.0f-ALPHA)*gy;
    filt_gz = ALPHA*filt_gz + (1.0f-ALPHA)*gz;

    out.ax = filt_ax; out.ay = filt_ay; out.az = filt_az;
    out.gx = filt_gx; out.gy = filt_gy; out.gz = filt_gz;
    return true;
}
