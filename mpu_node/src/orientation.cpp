#include "orientation.h"
#include <Arduino.h>
#include <math.h>

static const float DEG2RAD = (float)(M_PI / 180.0);
static Orientation g_orient;

// ── Madgwick ──────────────────────────────────────────────────────────────────
// Sebastian Madgwick's gradient-descent AHRS filter. unused for now.
// Fuses accel + gyro into a quaternion; roll/pitch/yaw extracted at the end.
// beta tunes gyro drift correction aggressiveness (0.1 is a common starting point).
//
// To use: call madgwickUpdate() instead of the atan2 block in orientationUpdate(),
// then replace g_orient.roll/pitch/yaw with the values from madgwickGetEuler().
//
// static float beta = 0.1f;
// static float q0 = 1.0f, q1 = 0.0f, q2 = 0.0f, q3 = 0.0f;
//
// static void madgwickUpdate(float ax, float ay, float az,
//                            float gx, float gy, float gz, float dt) {
//     float recipNorm;
//     float s0, s1, s2, s3;
//     float qDot0, qDot1, qDot2, qDot3;
//     float _2q0, _2q1, _2q2, _2q3;
//     float _4q0, _4q1, _4q2;
//     float _8q1, _8q2;
//     float q0q0, q1q1, q2q2, q3q3;
//
//     // gyro in rad/s already — if in deg/s multiply by DEG2RAD first
//     qDot0 = 0.5f * (-q1 * gx - q2 * gy - q3 * gz);
//     qDot1 = 0.5f * ( q0 * gx + q2 * gz - q3 * gy);
//     qDot2 = 0.5f * ( q0 * gy - q1 * gz + q3 * gx);
//     qDot3 = 0.5f * ( q0 * gz + q1 * gy - q2 * gx);
//
//     // apply feedback only if accel is non-zero
//     if (!((ax == 0.0f) && (ay == 0.0f) && (az == 0.0f))) {
//         recipNorm = 1.0f / sqrtf(ax * ax + ay * ay + az * az);
//         ax *= recipNorm; ay *= recipNorm; az *= recipNorm;
//
//         _2q0 = 2.0f * q0; _2q1 = 2.0f * q1;
//         _2q2 = 2.0f * q2; _2q3 = 2.0f * q3;
//         _4q0 = 4.0f * q0; _4q1 = 4.0f * q1; _4q2 = 4.0f * q2;
//         _8q1 = 8.0f * q1; _8q2 = 8.0f * q2;
//         q0q0 = q0 * q0; q1q1 = q1 * q1;
//         q2q2 = q2 * q2; q3q3 = q3 * q3;
//
//         s0 = _4q0 * q2q2 + _2q2 * ax + _4q0 * q1q1 - _2q1 * ay;
//         s1 = _4q1 * q3q3 - _2q3 * ax + 4.0f * q0q0 * q1 - _2q0 * ay
//              - _4q1 + _8q1 * q1q1 + _8q1 * q2q2 + _4q1 * az;
//         s2 = 4.0f * q0q0 * q2 + _2q0 * ax + _4q2 * q3q3 - _2q3 * ay
//              - _4q2 + _8q2 * q1q1 + _8q2 * q2q2 + _4q2 * az;
//         s3 = 4.0f * q1q1 * q3 - _2q1 * ax + 4.0f * q2q2 * q3 - _2q2 * ay;
//
//         recipNorm = 1.0f / sqrtf(s0 * s0 + s1 * s1 + s2 * s2 + s3 * s3);
//         s0 *= recipNorm; s1 *= recipNorm;
//         s2 *= recipNorm; s3 *= recipNorm;
//
//         qDot0 -= beta * s0;
//         qDot1 -= beta * s1;
//         qDot2 -= beta * s2;
//         qDot3 -= beta * s3;
//     }
//
//     q0 += qDot0 * dt;
//     q1 += qDot1 * dt;
//     q2 += qDot2 * dt;
//     q3 += qDot3 * dt;
//
//     recipNorm = 1.0f / sqrtf(q0 * q0 + q1 * q1 + q2 * q2 + q3 * q3);
//     q0 *= recipNorm; q1 *= recipNorm;
//     q2 *= recipNorm; q3 *= recipNorm;
// }
//
// static void madgwickGetEuler(float &roll, float &pitch, float &yaw) {
//     roll  = atan2f(2.0f * (q0 * q1 + q2 * q3),
//                    1.0f - 2.0f * (q1 * q1 + q2 * q2));
//     pitch = asinf( 2.0f * (q0 * q2 - q3 * q1));
//     yaw   = atan2f(2.0f * (q0 * q3 + q1 * q2),
//                    1.0f - 2.0f * (q2 * q2 + q3 * q3));
// }
// ─────────────────────────────────────────────────────────────────────────────

void orientationInit() {
    g_orient = {};
}

void orientationUpdate(float ax, float ay, float az,
                       float gx, float gy, float gz,
                       float dt) {
    g_orient.roll  = atan2f(ay, az);
    g_orient.pitch = atan2f(-ax, sqrtf(ay * ay + az * az));
    g_orient.yaw  += gz * DEG2RAD * dt;
}

Orientation orientationGet() {
    return g_orient;
}
