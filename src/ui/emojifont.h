#pragma once
#include <cstddef>
#include <cstdint>
// Reader for the emoji blob (tools/gen_emoji.py): 16x16 Twemoji, each with its own 16-colour
// palette. PURE, host-tested. The blob stays in flash; a lookup is a binary search over its keys.
// Format (little-endian): "EMJ1", u16 count, count x u32 key (ascending), then count glyphs of
// GLYPH_BYTES: 16 x u16 RGB565 palette (index 0 = transparent), 16x16 pixels at 4 bpp, row-major,
// high nibble = left pixel. Keys: the codepoint, or glyphrun::flagKey for a flag.
namespace emojifont {
constexpr int SIZE = 16;
constexpr size_t GLYPH_BYTES = 32 + 128;
class Font {
public:
    // Validates the "EMJ1" header, key order and size. False leaves the font empty.
    bool open(const uint8_t* blob, size_t len);
    // The glyph record for key, or nullptr.
    const uint8_t* glyph(uint32_t key) const;
    int count() const { return n_; }
private:
    uint32_t key(int i) const;
    const uint8_t* blob_ = nullptr;
    int n_ = 0;
};
uint16_t paletteAt(const uint8_t* glyph, int i);
int pixelAt(const uint8_t* glyph, int x, int y);   // palette index, 0 = transparent
}
