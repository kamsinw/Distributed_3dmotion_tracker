#pragma once

// Orientation angles in radians.
struct Orientation {
    float roll  = 0.0f;
    float pitch = 0.0f;
    float yaw   = 0.0f;
};

// Reset angles to zero and seed from first accel reading.
void orientationInit();

// Update orientation using complementary filter.
// ax/ay/az in m/s²,  gx/gy/gz in deg/s,  dt in seconds.
void orientationUpdate(float ax, float ay, float az,
                       float gx, float gy, float gz,
                       float dt);

Orientation orientationGet();
