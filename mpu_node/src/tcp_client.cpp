#include "tcp_client.h"
#include "config.h"
#include <WiFi.h>

static WiFiClient client;

static bool wifiConnect() {
    if (WiFi.status() == WL_CONNECTED) return true;

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

#ifdef DEBUG_MODE
    Serial.print("[WIFI] Connecting");
#endif

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 15000) {
#ifdef DEBUG_MODE
            Serial.println("\n[WIFI] Timeout");
#endif
            return false;
        }
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
    client.println(json);
    return true;
}

bool tcpReconnectIfNeeded() {
    if (client.connected()) return false;

#ifdef DEBUG_MODE
    Serial.println("[TCP] Lost connection — reconnecting...");
#endif
    client.stop();
    if (WiFi.status() != WL_CONNECTED) wifiConnect();
    bool ok = client.connect(SERVER_IP, SERVER_PORT);
#ifdef DEBUG_MODE
    if (ok) Serial.println("[TCP] Reconnected — position reset");
#endif
    return ok;
}
