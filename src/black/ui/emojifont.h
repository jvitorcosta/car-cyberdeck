#pragma once
#include <cstddef>
#include <cstdint>
// Reader for the emoji blob (tools/gen_emoji.py): 16x16 Twemoji, each with its own 16-colour
// palette. PURE, host-tested. The blob stays in flash; a lookup is a binary search over its keys.
// Format (little-endian): "EMJ2", u16 count, u16 nseq, count x u32 key (ascending), nseq
// sequences of SEQ_MAX x u32 codepoints (zero-padded, ascending), then count glyphs of
// GLYPH_BYTES: 16 x u16 RGB565 palette (index 0 = transparent), 16x16 pixels at 4 bpp, row-major,
// high nibble = left pixel. Keys: the codepoint, glyphrun::flagKey for a flag, SEQ_BASE + i for
// sequence i. "EMJ1" (u16 count, keys, glyphs: no sequences) still opens.
namespace emojifont {
constexpr int SIZE = 16;
constexpr size_t GLYPH_BYTES = 32 + 128;
// A joined (ZWJ) emoji, stored without ZWJ, variation selectors and skin tones: 👩‍💻 = 1F469 1F4BB.
constexpr int SEQ_MAX = 4;
constexpr uint32_t SEQ_BASE = 0x120000;
class Font {
public:
    // Validates the header, key and sequence order and size. False leaves the font empty.
    bool open(const uint8_t* blob, size_t len);
    // The glyph record for key, or nullptr.
    const uint8_t* glyph(uint32_t key) const;
    // The key of the sequence cps[0..n), or 0 when the font does not have it.
    uint32_t sequence(const uint32_t* cps, int n) const;
    int count() const { return n_; }
private:
    uint32_t key(int i) const;
    int compareSeq(int i, const uint32_t* cps, int n) const;   // like memcmp, zero-padded
    const uint8_t* blob_ = nullptr;
    const uint8_t* seqs_ = nullptr;
    const uint8_t* glyphs_ = nullptr;
    int n_ = 0, nseq_ = 0;
};
uint16_t paletteAt(const uint8_t* glyph, int i);
int pixelAt(const uint8_t* glyph, int x, int y);   // palette index, 0 = transparent
}
