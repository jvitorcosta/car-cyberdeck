#pragma once
#include <Arduino.h>
namespace net {
bool connectAny();
bool isOnline();
void loop();
String deviceIp();
String ssid();                            // connected network, "" when offline
void savedNetworks(char* out, size_t n);  // "Cecilia, iPhone" from config.h
}
