#pragma once
#include <stdbool.h>

void ultrasonicInit();
void triggerPulse();
bool ultrasonicRead(float &dist_cm);
