#pragma once
#include <Arduino.h>
namespace net {
bool connectAny();             // config.h order (the hotspot first; two tries after a drop from it)
// Move to the next saved network (falls back to connectAny if it isn't there). The companion
// uses it to follow the new deck onto its network.
bool switchNetwork();
bool isOnline();
void loop();
String deviceIp();
String ssid();                            // connected network, "" when offline
void savedNetworks(char* out, size_t n);  // "Cecilia, iPhone" from config.h
}
