#pragma once
#include <stdint.h>

// Filtered, bias-corrected IMU readings averaged across both sensors.
// Accelerations in m/s², gyro in deg/s.
struct RawImuData {
    float ax, ay, az;
    float gx, gy, gz;
};

// Initialize I2C bus and both MPU9250s (0x68 and 0x69).
// Returns false only if BOTH sensors fail — a single failure is tolerated.
bool mpuInit();

// Collect 'samples' readings from each active sensor to compute
// per-sensor accelerometer DC offsets.  Call once during setup().
void mpuCalibrate(int samples = 200);

// Poll both sensors, average, apply bias removal and LPF (alpha=0.7).
// Returns false only if no sensor produced new data this cycle.
bool mpuRead(RawImuData &out);

// Returns bitmask of which sensors initialised successfully.
// Bit 0 = sensor A (0x68), Bit 1 = sensor B (0x69).
uint8_t mpuActiveMask();
