#pragma once

struct Orientation {
    float roll  = 0.0f;
    float pitch = 0.0f;
    float yaw   = 0.0f;
};

void orientationInit();
void orientationUpdate(float ax, float ay, float az,
                       float gx, float gy, float gz,
                       float dt);
Orientation orientationGet();
