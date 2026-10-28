#pragma once
#include <cstddef>
#include <cstdint>
// A boot-scene car from a file (data/mcqueen.bin, tools/gen_mcqueen_sprite.py), read in place
// from flash. Format "CAR1" (little-endian): u16 w, h, rearX, frontX, wheelY (wheel centres),
// headX, headY (headlight beam origin), tailY (taillight glow top); then ceil(w*h/2) kind bytes
// (car_sprite::Kind, even pixel in the high nibble); then w*h u16 RGB565. PURE, host-tested.
namespace carbin {
struct Car { int w, h, rearX, frontX, wheelY, headX, headY, tailY; const uint8_t* kinds; const uint8_t* px; };
bool parse(const uint8_t* blob, size_t len, Car* out);   // false: not a whole, valid file
// As car_sprite::pixel: false when transparent or outside (*kind = 0 then).
bool pixel(const Car& c, int x, int y, uint16_t* rgb565, uint8_t* kind);
}
