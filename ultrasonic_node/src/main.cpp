#include <Arduino.h>
#include "config.h"
#include "ultrasonic_driver.h"
#include "distance_filter.h"
#include "tcp_client.h"

static uint32_t last_loop_ms = 0;
static char     json_buf[192];

void setup() {
    Serial.begin(115200);
    delay(500);

#ifdef DEBUG_MODE
    Serial.println("\n[SYS] ===  Ultrasonic Node  ===");
#endif

    ultrasonicInit();
    filterInit();
    tcpConnect();

    last_loop_ms = millis();
    triggerPulse();

#ifdef DEBUG_MODE
    Serial.println("[SYS] Ready");
#endif
}

void loop() {
    const uint32_t now = millis();

    if (now - last_loop_ms >= ULTRA_LOOP_MS) {
        const float dt = (float)(now - last_loop_ms) * 0.001f;
        last_loop_ms = now;

        float raw_cm;
        if (ultrasonicRead(raw_cm)) {
            float filtered = filterUpdate(raw_cm);
            float relative = getRelativeDistance();
            float speed    = getSpeed(filtered, dt);

#ifdef DEBUG_MODE
            Serial.printf("[ULTRA] raw=%.1f  filt=%.1f  rel=%.1f  spd=%.2f\n",
                          raw_cm, filtered, relative, speed);
#endif

            tcpReconnectIfNeeded();

            snprintf(json_buf, sizeof(json_buf),
                "{\"node\":\"ultrasonic\",\"timestamp\":%lu,"
                "\"distance\":%.2f,\"relative_distance\":%.2f,"
                "\"speed\":%.4f}",
                (unsigned long)millis(),
                filtered, relative, speed);

            sendJson(json_buf);
        }

        triggerPulse();
    }
}
