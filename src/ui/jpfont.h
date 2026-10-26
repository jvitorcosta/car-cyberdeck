#pragma once
#include <cstddef>
#include <cstdint>
// Reader for the yellow deck's Japanese Unifont subset (data/jp16.bin, tools/gen_jp_font.py): the
// kana, CJK punctuation, half/fullwidth forms and the JIS X 0208 level 1 kanji. The blob stays in
// flash; a lookup is a binary search over its sorted codepoints. PURE, host-tested.
namespace jpfont {
class Font {
public:
    // Validates the "JFNT" header, the codepoints' order, the widths and the size. False leaves
    // the font empty.
    bool open(const uint8_t* blob, size_t len);
    // 16-row bitmap for cp (width 8: 1 B/row, width 16: 2 B/row, MSB = leftmost) or nullptr.
    const uint8_t* glyph(uint32_t cp, int* width) const;
    // cp's glyph width (8 or 16 px), 0 when the font doesn't have it.
    int width(uint32_t cp) const;
    int count() const { return n_; }
private:
    int find(uint32_t cp) const;  // index of cp, -1 if absent
    const uint8_t* blob_ = nullptr;
    int n_ = 0;
};
}
