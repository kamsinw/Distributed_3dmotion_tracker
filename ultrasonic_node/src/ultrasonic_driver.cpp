#include "ultrasonic_driver.h"
#include "config.h"
#include <Arduino.h>

static volatile uint32_t echo_start_us = 0;
static volatile uint32_t echo_end_us   = 0;
static volatile bool     echo_ready    = false;

static uint32_t trig_fired_ms = 0;

static void IRAM_ATTR echoISR() {
    if (digitalRead(ECHO_PIN)) {
        echo_start_us = micros();
    } else {
        echo_end_us = micros();
        echo_ready  = true;
    }
}

void ultrasonicInit() {
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    digitalWrite(TRIG_PIN, LOW);

    attachInterrupt(digitalPinToInterrupt(ECHO_PIN), echoISR, CHANGE);
}

void triggerPulse() {
    echo_ready    = false;
    trig_fired_ms = millis();

    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
}

bool ultrasonicRead(float &dist_cm) {
    if (!echo_ready) {
        if (millis() - trig_fired_ms > 60) {
            trig_fired_ms = millis();
        }
        return false;
    }

    noInterrupts();
    uint32_t start = echo_start_us;
    uint32_t end   = echo_end_us;
    echo_ready     = false;
    interrupts();

    uint32_t duration_us = end - start;
    dist_cm = (float)duration_us * 0.034f / 2.0f;

    if (dist_cm < 2.0f || dist_cm > 400.0f) {
        return false;
    }

    return true;
}
