#pragma once
#include <cstdint>
// The boot terminal over the kiss video (ui/intro): composited into each JPEG block as it's
// decoded, so the video never wipes it and there's no second pass. PURE, host-tested.
namespace termblend {
constexpr uint16_t KEY = 0x0120;     // overlay "no glyph" (big-endian 0x2001: a green no text uses)
constexpr uint8_t SHADE = 100;       // the panel: the video at ~40 % under the text
void composite(uint16_t* block, int bx, int by, int bw, int bh,
               const uint16_t* overlay, int ox, int oy, int ow, int oh);
}
