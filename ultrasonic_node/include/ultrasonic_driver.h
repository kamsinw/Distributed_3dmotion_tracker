#pragma once
#include <stdbool.h>

// Configure TRIG/ECHO pins and attach CHANGE interrupt on ECHO.
void ultrasonicInit();

// Fire a 10 µs TRIG pulse.  Call once per measurement cycle.
// Must not be called from an ISR.
void triggerPulse();

// Returns true (and sets dist_cm) when the ECHO interrupt pair
// has completed since the last triggerPulse() call.
// Returns false when measurement is still in-flight or timed out.
bool ultrasonicRead(float &dist_cm);
