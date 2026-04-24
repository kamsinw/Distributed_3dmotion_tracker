#pragma once

enum MotionState {
    STATE_IDLE  = 0,
    STATE_MOVE  = 1,
    STATE_SHAKE = 2,
    STATE_DROP  = 3
};

// Classify motion based on acceleration statistics.
MotionState classify(float std_accel, float max_accel);

// Return the JSON-ready string label for a state.
const char* stateToString(MotionState state);
