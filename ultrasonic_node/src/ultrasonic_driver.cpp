#include "ultrasonic_driver.h"
#include "config.h"
#include <Arduino.h>

// ------------------------------------------------------------------
// Volatile ISR state — shared between ISR and main loop.
// 'volatile' prevents the compiler from caching these in registers.
// ------------------------------------------------------------------
static volatile uint32_t echo_start_us = 0;   // µs timestamp at rising edge
static volatile uint32_t echo_end_us   = 0;   // µs timestamp at falling edge
static volatile bool     echo_ready    = false; // set true after falling edge

static uint32_t trig_fired_ms = 0;            // millis() when last TRIG sent

// ------------------------------------------------------------------
// ISR — single handler for both RISING and FALLING edges.
// Placed in IRAM so it can execute without flash cache access.
// ------------------------------------------------------------------
static void IRAM_ATTR echoISR() {
    if (digitalRead(ECHO_PIN)) {
        // Rising edge — start timer
        echo_start_us = micros();
    } else {
        // Falling edge — capture duration and signal main loop
        echo_end_us = micros();
        echo_ready  = true;
    }
}

// ------------------------------------------------------------------
// Public API
// ------------------------------------------------------------------

void ultrasonicInit() {
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    digitalWrite(TRIG_PIN, LOW);

    // Attach a single ISR that handles both edge directions
    attachInterrupt(digitalPinToInterrupt(ECHO_PIN), echoISR, CHANGE);
}

void triggerPulse() {
    // Clear previous result before firing so we don't read stale data
    echo_ready    = false;
    trig_fired_ms = millis();

    // HC-SR04 TRIG sequence: LOW → HIGH (≥10 µs) → LOW
    // delayMicroseconds(10) is a µs-scale hardware requirement,
    // NOT a long blocking delay — it is unavoidable and acceptable.
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
}

bool ultrasonicRead(float &dist_cm) {
    // Timeout guard — 60 ms covers the max HC-SR04 echo for 400 cm
    if (!echo_ready) {
        if (millis() - trig_fired_ms > 60) {
            // No echo received; reset timer so next cycle is clean
            trig_fired_ms = millis();
        }
        return false;
    }

    // Snapshot and clear the ready flag atomically (disable IRQ briefly)
    noInterrupts();
    uint32_t start = echo_start_us;
    uint32_t end   = echo_end_us;
    echo_ready     = false;
    interrupts();

    uint32_t duration_us = end - start;
    dist_cm = (float)duration_us * 0.034f / 2.0f;

    // Sanity bounds: HC-SR04 range 2 cm – 400 cm
    if (dist_cm < 2.0f || dist_cm > 400.0f) {
        return false;
    }

    return true;
}
