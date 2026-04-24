#include "distance_filter.h"
#include "circular_buffer.h"
#include "config.h"

// 5-sample sliding window — enough to smooth HC-SR04 noise
// without adding significant lag at 20 Hz
static CircularBuffer<float, 5> dist_buf;

// Baseline distance captured at startup (cm)
static float baseline_cm = -1.0f;  // negative = not yet set

// Previous filtered reading — used to compute speed
static float prev_filtered = 0.0f;

// ------------------------------------------------------------------

void filterInit() {
    dist_buf.clear();
    baseline_cm   = -1.0f;
    prev_filtered = 0.0f;
}

float filterUpdate(float raw_cm) {
    dist_buf.push(raw_cm);

    // Capture baseline once the window has at least 3 samples
    if (baseline_cm < 0.0f && dist_buf.count >= 3) {
        float sum = 0.0f;
        for (uint16_t i = 0; i < dist_buf.count; i++) sum += dist_buf.at(i);
        baseline_cm = sum / (float)dist_buf.count;

#ifdef DEBUG_MODE
        Serial.printf("[FILT] Baseline set: %.1f cm\n", baseline_cm);
#endif
    }

    // Sliding window mean
    float sum = 0.0f;
    for (uint16_t i = 0; i < dist_buf.count; i++) sum += dist_buf.at(i);
    return sum / (float)dist_buf.count;
}

float getRelativeDistance() {
    if (baseline_cm < 0.0f || dist_buf.count == 0) return 0.0f;

    float sum = 0.0f;
    for (uint16_t i = 0; i < dist_buf.count; i++) sum += dist_buf.at(i);
    return (sum / (float)dist_buf.count) - baseline_cm;
}

float getSpeed(float filtered_cm, float dt) {
    if (dt <= 0.0f) {
        prev_filtered = filtered_cm;
        return 0.0f;
    }
    float speed   = (filtered_cm - prev_filtered) / dt;
    prev_filtered = filtered_cm;
    return speed;
}
