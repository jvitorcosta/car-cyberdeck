#pragma once
#include <cstdint>
// RGB565 colour helpers shared by the UI, the boot scene and the image decoders. PURE.
namespace rgb565 {
// 8-bit-per-channel colour -> RGB565 (what TFT_eSPI fill/draw calls take).
constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
// 0xRRGGBB -> RGB565.
constexpr uint16_t hex(uint32_t c) { return rgb((uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c); }
// Bytes swapped: TFT_eSPI pushImage() buffers are big-endian.
constexpr uint16_t be(uint16_t c) { return (uint16_t)((c >> 8) | (c << 8)); }
}
