#include <Arduino.h>
#include "config.h"
#include "mpu_driver.h"
#include "orientation.h"
#include "feature_extraction.h"
#include "classifier.h"
#include "tcp_client.h"

// millis() timestamps for non-blocking scheduling
static uint32_t last_mpu_ms  = 0;
static uint32_t last_send_ms = 0;

// Reuse a single stack-allocated buffer for JSON serialisation
static char json_buf[256];

// ------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(500);  // allow USB-serial to settle

#ifdef DEBUG_MODE
    Serial.println("\n[SYS] ===  MPU Node  ===");
#endif

    if (!mpuInit()) {
#ifdef DEBUG_MODE
        Serial.println("[SYS] Both MPU9250s failed — check wiring. Halting.");
#endif
        while (true) yield();
    }

#ifdef DEBUG_MODE
    uint8_t mask = mpuActiveMask();
    Serial.printf("[SYS] Active sensors: A(0x68)=%s  B(0x69)=%s\n",
                  (mask & 0x01) ? "YES" : "no",
                  (mask & 0x02) ? "YES" : "no");
    Serial.println("[SYS] Calibrating — keep both sensors still...");
#endif
    mpuCalibrate(200);

    orientationInit();
    featureInit();

    tcpConnect();

    last_mpu_ms  = millis();
    last_send_ms = millis();

#ifdef DEBUG_MODE
    Serial.println("[SYS] Ready");
#endif
}

// ------------------------------------------------------------------

void loop() {
    const uint32_t now = millis();

    // ---- Sensor pipeline at 50 Hz (every 20 ms) ------------------
    if (now - last_mpu_ms >= MPU_LOOP_MS) {
        const float dt = (float)(now - last_mpu_ms) * 0.001f;
        last_mpu_ms = now;

        RawImuData imu;
        if (mpuRead(imu)) {
            orientationUpdate(imu.ax, imu.ay, imu.az,
                              imu.gx, imu.gy, imu.gz, dt);
            featureUpdate(imu.ax, imu.ay, imu.az, dt);
        }
    }

    // ---- TCP send at 20 Hz (every 50 ms) -------------------------
    if (now - last_send_ms >= SEND_LOOP_MS) {
        last_send_ms = now;

        tcpReconnectIfNeeded();

        const Orientation o = orientationGet();
        const Features    f = featureGet();
        const MotionState s = classify(f.std_accel, f.max_accel);

        snprintf(json_buf, sizeof(json_buf),
            "{\"node\":\"mpu\",\"timestamp\":%lu,"
            "\"roll\":%.4f,\"pitch\":%.4f,\"yaw\":%.4f,"
            "\"velocity\":%.4f,\"accel_mag\":%.4f,"
            "\"state\":\"%s\"}",
            (unsigned long)millis(),
            o.roll, o.pitch, o.yaw,
            f.velocity, f.accel_mag,
            stateToString(s));

        sendJson(json_buf);

#ifdef DEBUG_MODE
        Serial.printf("[TX] roll=%.2f  pitch=%.2f  yaw=%.2f  state=%s\n",
                      o.roll * 57.2958f,
                      o.pitch * 57.2958f,
                      o.yaw   * 57.2958f,
                      stateToString(s));
#endif
    }
}
