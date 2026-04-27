// Direct trig orientation — inspired by the approach from Claude's ESP-IDF example.
//
// Roll and pitch are computed directly from the accelerometer using atan2.
// They are stable, instant, and never drift — no filter needed.
// Yaw is integrated from the gyro Z axis and will drift slowly over time,
// which is acceptable for a real-time demo.

#include "orientation.h"
#include <Arduino.h>
#include <math.h>

static const float DEG2RAD = (float)(M_PI / 180.0);

static Orientation g_orient;

void orientationInit() {
    g_orient = {};
}

void orientationUpdate(float ax, float ay, float az,
                       float gx, float gy, float gz,
                       float dt) {
    // Roll: rotation about X axis — stable from accel
    g_orient.roll  = atan2f(ay, az);

    // Pitch: rotation about Y axis — stable from accel
    g_orient.pitch = atan2f(-ax, sqrtf(ay * ay + az * az));

    // Yaw: integrate gyro Z (gz is in deg/s, convert to rad/s first)
    g_orient.yaw  += gz * DEG2RAD * dt;
}

Orientation orientationGet() {
    return g_orient;
}
