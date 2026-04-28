#pragma once

struct Features {
    float mean_accel = 0.0f;
    float std_accel  = 0.0f;
    float max_accel  = 0.0f;
    float velocity   = 0.0f;
    float accel_mag  = 0.0f;
};

void     featureInit();
void     featureUpdate(float ax, float ay, float az, float dt);
Features featureGet();
