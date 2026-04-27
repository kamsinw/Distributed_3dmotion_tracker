#include "classifier.h"
#include "config.h"

// Thresholds (m/s² units)
// Serial logs show std ~0.01-0.05 at rest, 0.05-0.20 during gentle movement.
static constexpr float IDLE_STD  = 0.05f;
static constexpr float SHAKE_STD = 1.2f;
static constexpr float DROP_ACC  = 3.5f;

MotionState classify(float std_accel, float max_accel) {
    if (std_accel < IDLE_STD) {
        return STATE_IDLE;
    }
    if (max_accel > DROP_ACC) {
        return STATE_DROP;
    }
    if (std_accel > SHAKE_STD) {
        return STATE_SHAKE;
    }
    return STATE_MOVE;
}

const char* stateToString(MotionState state) {
    switch (state) {
        case STATE_IDLE:  return "IDLE";
        case STATE_MOVE:  return "MOVE";
        case STATE_SHAKE: return "SHAKE";
        case STATE_DROP:  return "DROP";
        default:          return "IDLE";
    }
}
