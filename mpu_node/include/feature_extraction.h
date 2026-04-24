#pragma once

struct Features {
    float mean_accel = 0.0f;
    float std_accel  = 0.0f;
    float max_accel  = 0.0f;
    float velocity   = 0.0f;
    float accel_mag  = 0.0f;
};

void     featureInit();

// Push latest accelerometer reading into the circular buffer,
// recompute statistics, and integrate velocity.
// ax/ay/az in m/s²,  dt in seconds.
void     featureUpdate(float ax, float ay, float az, float dt);

Features featureGet();
