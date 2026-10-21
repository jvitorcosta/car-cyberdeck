#pragma once
// Command frames for the car's ELK-family BLE LED controllers (ELK-BLEDDM, MELK-OA): 9 bytes,
// 7E ... EF, written without response to characteristic FFF3 (service FFF0). PURE, host-tested.
// Two header variants exist in the wild; the deck sends both and each controller ignores the other.
#include <stdint.h>

namespace elkcmd {
struct Frame { uint8_t b[9]; };

enum Variant { V0, V1 };  // V0: 7E 00 ..., V1: 7E 07 (colour) / 7E 04 (power)

inline Frame color(Variant v, uint8_t r, uint8_t g, uint8_t b) {
    if (v == V0) return {{0x7E, 0x00, 0x05, 0x03, r, g, b, 0x00, 0xEF}};
    return {{0x7E, 0x07, 0x05, 0x03, r, g, b, 0x10, 0xEF}};
}

inline Frame power(Variant v, bool on) {
    const uint8_t s = on ? 0xF0 : 0x00, t = on ? 0x01 : 0x00;
    return {{0x7E, static_cast<uint8_t>(v == V0 ? 0x00 : 0x04), 0x04, s, 0x00, t, 0xFF, 0x00, 0xEF}};
}

// Intensity by scaling the colour itself (0..100 %): the colour command is the one proven on the
// car's controllers. A channel that is on never rounds down to 0, so a dim colour keeps its hue.
inline uint8_t dim(uint8_t channel, uint8_t pct) {
    if (pct >= 100) return channel;
    if (channel == 0 || pct == 0) return 0;
    const unsigned v = (channel * pct + 50) / 100;
    return static_cast<uint8_t>(v == 0 ? 1 : v);
}
}
