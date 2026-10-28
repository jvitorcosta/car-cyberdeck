#pragma once
#include <cstdint>
// The boot terminal over the kiss video (ui/intro): composited into each JPEG block as it's
// decoded, so the video never wipes it and there's no second pass. PURE, host-tested.
namespace termblend {
constexpr uint16_t KEY = 0x0120;     // overlay "no glyph" (big-endian 0x2001: a green no text uses)
// The panel under the text: the video at half brightness (c/2 per channel), by a shift and mask;
// a per-channel multiply here cost ~5 ms a frame on the S3, ~36 of the clip's 462 frames.
constexpr uint16_t shade(uint16_t c) { return (uint16_t)((c >> 1) & 0x7BEF); }
void composite(uint16_t* block, int bx, int by, int bw, int bh,
               const uint16_t* overlay, int ox, int oy, int ow, int oh);
}
