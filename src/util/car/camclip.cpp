#include "camclip.h"
#include <cstring>

namespace camclip {

static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

Slot limitSlot(uint8_t limit) {
    switch (limit) {
        case 20: return Slot::L20;
        case 30: return Slot::L30;
        case 40: return Slot::L40;
        case 50: return Slot::L50;
        case 60: return Slot::L60;
        case 80: return Slot::L80;
        default: return Slot::COUNT;
    }
}

uint32_t lengthMs(const Clip& c) { return (uint32_t)((uint64_t)c.samples * 1000 / RATE); }

bool Table::load(const uint8_t* blob, size_t len) {
    for (Clip& c : clips_) c = {nullptr, 0};
    if (!blob || len < HEAD || memcmp(blob, "CAMV", 4) != 0 || rd16(blob + 4) != VERSION) return false;
    const size_t count = rd16(blob + 6);
    const size_t table = HEAD + ENTRY * count;
    if (len < table) return false;
    for (size_t i = 0; i < count && i < (size_t)COUNT; i++) {
        const uint32_t off = rd32(blob + HEAD + ENTRY * i);
        const uint32_t size = rd32(blob + HEAD + ENTRY * i + 4);
        if (!size || size % 2 || off < table || off > len || size > len - off) continue;
        clips_[i] = {blob + off, size / 2};
    }
    return true;
}

Clip Table::get(Slot s) const { return (int)s < COUNT ? clips_[(int)s] : Clip{nullptr, 0}; }

uint16_t Table::present() const {
    uint16_t m = 0;
    for (int i = 0; i < COUNT; i++)
        if (clips_[i].samples) m |= (uint16_t)(1u << i);
    return m;
}

Reader::Reader(const Clip& c, int rate, int volume)
    : c_(c), rate_(rate > 0 ? (uint32_t)rate : (uint32_t)RATE), volume_(volume) {}

int16_t Reader::at(size_t i) const {
    int16_t s;
    memcpy(&s, c_.pcm + 2 * i, 2);
    return s;
}

size_t Reader::fill(int16_t* out, size_t n) {
    size_t k = 0;
    for (; k < n && i_ < c_.samples; ++k) {
        const int32_t a = at(i_);
        const int32_t b = i_ + 1 < c_.samples ? at(i_ + 1) : a;   // the last one held
        const int32_t s = a + (int32_t)((int64_t)(b - a) * frac_ / rate_);
        out[k] = (int16_t)(s * volume_ / 100);
        // One output sample on: RATE / rate_ of a source sample, kept exact as a fraction.
        frac_ += RATE;
        while (frac_ >= rate_) {
            frac_ -= rate_;
            ++i_;
        }
    }
    return k;
}

}  // namespace camclip
