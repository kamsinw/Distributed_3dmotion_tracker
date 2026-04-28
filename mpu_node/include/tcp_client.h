#pragma once
#include <Arduino.h>

bool tcpConnect();
bool tcpConnected();
bool sendJson(const char* json);
bool tcpReconnectIfNeeded();
