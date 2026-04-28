#pragma once

enum MotionState {
    STATE_IDLE  = 0,
    STATE_MOVE  = 1,
    STATE_SHAKE = 2,
    STATE_DROP  = 3
};

MotionState classify(float std_accel, float max_accel);
const char* stateToString(MotionState state);
