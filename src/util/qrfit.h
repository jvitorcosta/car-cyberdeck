#pragma once
#include <cstddef>
// Which QR code version holds the phone-remote link, drawn in the sky left of Pikachu on the
// "Nothing playing" screen (ui/statusscene): 2 px per module and the standard 4-module white
// quiet zone, which phone cameras need. Byte mode, error correction L. PURE, host-tested.
namespace qrfit {
constexpr int MAX_VERSION = 3;          // 29 modules: 74 px with the quiet zone
constexpr int MODULE_PX = 2, QUIET = 4;
// Smallest version 1..MAX_VERSION that holds `bytes`; 0 when empty or too long.
int version(size_t bytes);
inline int modules(int v) { return 17 + 4 * v; }
inline int pixels(int v) { return (modules(v) + 2 * QUIET) * MODULE_PX; }
}
