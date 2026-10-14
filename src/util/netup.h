#pragma once
#include <cstdint>
// When the WiFi link can carry traffic: connected AND holding an address. The driver can report
// connected with 0.0.0.0 (seen after an auto-reconnect to the hotspot: 40 s of failed DNS), and
// treating that as offline lets net::loop() run a clean reconnect. PURE, host-tested.
namespace netup {
inline bool usable(bool connected, uint32_t ip) { return connected && ip != 0; }
}
