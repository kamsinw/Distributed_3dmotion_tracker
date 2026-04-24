#include "orientation.h"
#include "config.h"
#include <Arduino.h>
#include <math.h>

static Orientation orient;
static bool first_update = true;

void orientationInit() {
    orient.roll  = 0.0f;
    orient.pitch = 0.0f;
    orient.yaw   = 0.0f;
    first_update = true;
}

void orientationUpdate(float ax, float ay, float az,
                       float gx, float gy, float gz,
                       float dt) {
    // Convert gyro from deg/s → rad/s
    const float DEG2RAD = (float)(M_PI / 180.0);
    float gx_r = gx * DEG2RAD;
    float gy_r = gy * DEG2RAD;
    float gz_r = gz * DEG2RAD;

    // Accelerometer-derived roll and pitch (rad)
    float accel_roll  = atan2f(ay, az);
    float accel_pitch = atan2f(-ax, sqrtf(ay * ay + az * az));

    if (first_update) {
        // Seed from accelerometer so we start near correct angles
        orient.roll  = accel_roll;
        orient.pitch = accel_pitch;
        orient.yaw   = 0.0f;
        first_update = false;
        return;
    }

    // Complementary filter — 98% gyro integration, 2% accel correction
    orient.roll  = 0.98f * (orient.roll  + gx_r * dt) + 0.02f * accel_roll;
    orient.pitch = 0.98f * (orient.pitch + gy_r * dt) + 0.02f * accel_pitch;

    // Yaw: gyro-only integration (no magnetometer reference here)
    orient.yaw += gz_r * dt;

    // Wrap yaw to [-π, π]
    while (orient.yaw >  (float)M_PI) orient.yaw -= 2.0f * (float)M_PI;
    while (orient.yaw < -(float)M_PI) orient.yaw += 2.0f * (float)M_PI;

#ifdef DEBUG_MODE
    Serial.printf("[ORI] roll=%.3f  pitch=%.3f  yaw=%.3f  (deg)\n",
                  orient.roll  * 57.2958f,
                  orient.pitch * 57.2958f,
                  orient.yaw   * 57.2958f);
#endif
}

Orientation orientationGet() { return orient; }
