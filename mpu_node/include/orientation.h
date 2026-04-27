#pragma once

// Orientation angles in radians.
struct Orientation {
    float roll  = 0.0f;
    float pitch = 0.0f;
    float yaw   = 0.0f;
};

// Reset yaw to zero.
void orientationInit();

// Update orientation:
//   roll  — directly from atan2(ay, az)         (no drift)
//   pitch — directly from atan2(-ax, |ay,az|)   (no drift)
//   yaw   — integrated from gz                  (drifts slowly)
// ax/ay/az in m/s²,  gx/gy/gz in deg/s,  dt in seconds.
void orientationUpdate(float ax, float ay, float az,
                       float gx, float gy, float gz,
                       float dt);

Orientation orientationGet();
