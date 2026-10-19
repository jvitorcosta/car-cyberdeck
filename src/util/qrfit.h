#pragma once
#include <cstddef>
// The phone-remote link as a QR code, drawn in the sky left of Sonic (or Pikachu) on the
// "Spotify is silent" screen (ui/statusscene): 2 px per module and the standard 4-module white
// quiet zone, which phone cameras need. Byte mode, error correction L. PURE, host-tested.
namespace qrfit {
constexpr int MAX_VERSION = 3;          // 29 modules: 74 px with the quiet zone
constexpr int MODULE_PX = 2, QUIET = 4;
// Smallest version 1..MAX_VERSION that holds `bytes`; 0 when empty or too long.
int version(size_t bytes);
constexpr int modules(int v) { return 17 + 4 * v; }
constexpr int pixels(int v) { return (modules(v) + 2 * QUIET) * MODULE_PX; }
// "http://<ip>/?k=<token>" from the deck's current address (172.20.10.10 on the iPhone hotspot,
// the DHCP one elsewhere). False (and "") with no token, no address (offline: "0.0.0.0"), or a
// link that doesn't fit `out` or a MAX_VERSION code.
bool link(const char* ip, const char* token, char* out, size_t n);
}
