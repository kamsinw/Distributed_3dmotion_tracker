#pragma once
#include <Arduino.h>

// Connect to WiFi and open TCP socket to server.
// Blocks during initial WiFi join (setup context only).
bool tcpConnect();

// Returns true if TCP socket is currently open.
bool tcpConnected();

// Send a null-terminated JSON string followed by '\n'.
// Returns false if the socket is not connected.
bool sendJson(const char* json);

// Check connection state; attempt reconnect if dropped.
// Returns true the first time after a successful reconnect (one-shot flag).
// Call from the main loop — non-blocking.
bool tcpReconnectIfNeeded();
