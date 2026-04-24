#pragma once

// Initialise the sliding-window filter and reset baseline.
void  filterInit();

// Push a new raw distance reading into the circular buffer.
// Returns the sliding-window mean (filtered distance in cm).
float filterUpdate(float raw_cm);

// Returns (filtered_distance − baseline).
// Baseline is captured automatically after the first 3 valid reads.
float getRelativeDistance();

// Returns instantaneous speed in cm/s given the current filtered
// reading and the elapsed time dt (seconds).
float getSpeed(float filtered_cm, float dt);
