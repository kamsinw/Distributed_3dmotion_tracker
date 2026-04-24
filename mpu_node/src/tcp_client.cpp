#include "tcp_client.h"
#include "config.h"
#include <WiFi.h>

static WiFiClient client;

// ------------------------------------------------------------------
// Internal helpers
// ------------------------------------------------------------------

static bool wifiConnect() {
    if (WiFi.status() == WL_CONNECTED) return true;

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

#ifdef DEBUG_MODE
    Serial.print("[WIFI] Connecting");
#endif

    // Block only during setup() — use millis() to avoid watchdog reset
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 15000) {
#ifdef DEBUG_MODE
            Serial.println("\n[WIFI] Timeout");
#endif
            return false;
        }
        // yield() surrenders to the RTOS; no fixed-duration block
        yield();
#ifdef DEBUG_MODE
        Serial.print('.');
#endif
    }

#ifdef DEBUG_MODE
    Serial.printf("\n[WIFI] Connected — IP: %s\n",
                  WiFi.localIP().toString().c_str());
#endif
    return true;
}

// ------------------------------------------------------------------
// Public API
// ------------------------------------------------------------------

bool tcpConnect() {
    if (!wifiConnect()) return false;

    if (client.connected()) return true;

    if (!client.connect(SERVER_IP, SERVER_PORT)) {
#ifdef DEBUG_MODE
        Serial.printf("[TCP] Failed to connect to %s:%d\n",
                      SERVER_IP, SERVER_PORT);
#endif
        return false;
    }

#ifdef DEBUG_MODE
    Serial.printf("[TCP] Connected to %s:%d\n", SERVER_IP, SERVER_PORT);
#endif
    return true;
}

bool tcpConnected() {
    return client.connected();
}

bool sendJson(const char* json) {
    if (!client.connected()) return false;
    // println() appends '\n' — server reads line-by-line
    client.println(json);
    return true;
}

void tcpReconnectIfNeeded() {
    if (!client.connected()) {
#ifdef DEBUG_MODE
        Serial.println("[TCP] Lost connection — reconnecting...");
#endif
        client.stop();
        // Non-blocking: if WiFi dropped, wifiConnect() will timeout
        // and we will retry next cycle rather than stalling the loop.
        if (WiFi.status() != WL_CONNECTED) {
            wifiConnect();
        }
        client.connect(SERVER_IP, SERVER_PORT);
    }
}
