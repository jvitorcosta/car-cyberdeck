// The terminal look's palette: green phosphor on black, with a dimmed night variant. PURE,
// host-tested.
#pragma once
#include <stdint.h>

namespace termpal {

enum Role : uint8_t { BG, TEXT, MID, DIM };

// Day: BG 0x000000, TEXT 0x3CFF7A, MID 0x1F9E48, DIM 0x0F5024. Night: each channel x45/100.
uint32_t rgb(Role r, bool night);
// A colour tile on the screen (e.g. a swatch): day unchanged; night each channel x70/100.
uint32_t tile(uint32_t screenRgb, bool night);

}  // namespace termpal
