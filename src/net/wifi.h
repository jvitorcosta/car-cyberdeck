#pragma once
#include <Arduino.h>
namespace net {
bool connectAny();             // config.h order (the hotspot first; two tries after a drop from it)
// Move to the next saved network (falls back to connectAny if it isn't there). The companion
// uses it to follow the new deck onto its network.
bool switchNetwork();
// Roaming back to the preferred network from a fallback (default on). The companion turns it
// off: it follows the new deck instead (util/follow), or the two could split.
void setRoaming(bool on);
bool isOnline();
void loop();
String deviceIp();
String ssid();                            // connected network, "" when offline
void savedNetworks(char* out, size_t n);  // "Cecilia, iPhone" from config.h
}
