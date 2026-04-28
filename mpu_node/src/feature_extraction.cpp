#include "feature_extraction.h"
#include "circular_buffer.h"
#include "config.h"
#include <Arduino.h>
#include <math.h>

static CircularBuffer<float, 50> accel_buf;
static float velocity = 0.0f;
static Features current;

void featureInit() {
    accel_buf.clear();
    velocity = 0.0f;
    current  = {};
}

void featureUpdate(float ax, float ay, float az, float dt) {
    float mag          = sqrtf(ax * ax + ay * ay + az * az);
    float motion_accel = mag - 9.81f;

    velocity += motion_accel * dt;
    velocity *= 0.98f;

    accel_buf.push(mag);

    current.accel_mag = mag;
    current.velocity  = velocity;

    if (accel_buf.count == 0) return;

    float sum     = 0.0f;
    float max_val = accel_buf.at(0);

    for (uint16_t i = 0; i < accel_buf.count; i++) {
        float v = accel_buf.at(i);
        sum += v;
        if (v > max_val) max_val = v;
    }
    float mean = sum / (float)accel_buf.count;

    float var_sum = 0.0f;
    for (uint16_t i = 0; i < accel_buf.count; i++) {
        float diff = accel_buf.at(i) - mean;
        var_sum += diff * diff;
    }

    current.mean_accel = mean;
    current.std_accel  = sqrtf(var_sum / (float)accel_buf.count);
    current.max_accel  = max_val;

#ifdef DEBUG_MODE
    Serial.printf("[FEAT] mean=%.3f  std=%.3f  max=%.3f  vel=%.3f\n",
                  current.mean_accel, current.std_accel,
                  current.max_accel,  current.velocity);
#endif
}

Features featureGet() { return current; }
