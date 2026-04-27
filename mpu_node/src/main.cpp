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

// Latest calibrated accelerometer readings (m/s²), updated at 50 Hz.
// Read by the 20 Hz send loop to include in the JSON packet.
static float g_ax = 0.0f, g_ay = 0.0f, g_az = 0.0f;

static char json_buf[256];

// ------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(500);

#ifdef DEBUG_MODE
    Serial.println("\n[SYS] ===  MPU Node  ===");
#endif

    if (!mpuInit()) {
#ifdef DEBUG_MODE
        Serial.println("[SYS] Sensor not found — check wiring. Halting.");
#endif
        while (true) yield();
    }

#ifdef DEBUG_MODE
    Serial.println("[SYS] Calibrating — keep sensor still...");
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

    // ---- Sensor pipeline at 50 Hz --------------------------------
    if (now - last_mpu_ms >= MPU_LOOP_MS) {
        last_mpu_ms = now;

        RawImuData imu;
        if (mpuRead(imu)) {
            // Store latest accelerometer readings for the send loop
            g_ax = imu.ax;
            g_ay = imu.ay;
            g_az = imu.az;

            const float dt = (float)MPU_LOOP_MS * 0.001f;
            orientationUpdate(imu.ax, imu.ay, imu.az,
                              imu.gx, imu.gy, imu.gz, dt);
            featureUpdate(imu.ax, imu.ay, imu.az, dt);
        }
    }

    // ---- TCP send at 20 Hz ---------------------------------------
    if (now - last_send_ms >= SEND_LOOP_MS) {
        last_send_ms = now;

        tcpReconnectIfNeeded();

        const Orientation o = orientationGet();
        const Features    f = featureGet();
        const MotionState s = classify(f.std_accel, f.max_accel);

        snprintf(json_buf, sizeof(json_buf),
            "{\"node\":\"mpu\",\"timestamp\":%lu,"
            "\"ax\":%.4f,\"ay\":%.4f,\"az\":%.4f,"
            "\"roll\":%.4f,\"pitch\":%.4f,\"yaw\":%.4f,"
            "\"velocity\":%.4f,\"accel_mag\":%.4f,"
            "\"state\":\"%s\"}",
            (unsigned long)millis(),
            g_ax, g_ay, g_az,
            o.roll, o.pitch, o.yaw,
            f.velocity, f.accel_mag,
            stateToString(s));

        sendJson(json_buf);

#ifdef DEBUG_MODE
        Serial.printf("[TX] ax=%.3f ay=%.3f az=%.3f "
                      "roll=%.1f pitch=%.1f yaw=%.1f state=%s\n",
                      g_ax, g_ay, g_az,
                      o.roll*57.3f, o.pitch*57.3f, o.yaw*57.3f,
                      stateToString(s));
#endif
    }
}
