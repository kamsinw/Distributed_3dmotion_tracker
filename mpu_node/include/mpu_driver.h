#pragma once
#include <stdint.h>

struct RawImuData {
    float ax, ay, az;
    float gx, gy, gz;
};

bool mpuInit();
void mpuCalibrate(int samples = 200);
bool mpuRead(RawImuData &out);
bool mpuReadPos(RawImuData &out);
uint8_t mpuActiveMask();
