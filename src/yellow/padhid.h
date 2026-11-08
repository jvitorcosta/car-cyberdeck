#pragma once
// The yellow deck's own output report to the PS3 controller (rumble and the four player LEDs), in
// place of esp32-ps3's ps3Cmd: that one takes a 4 KB buffer for each ~50-byte report and writes into
// it even when the allocation failed, which crashed the deck on a heap this tight (Bluedroid's dual
// mode leaves ~20 KB). This one allocates what the report needs and skips it when it can't.
#include <cstdint>

namespace padhid {
// leds: bit 0 = LED 1 .. bit 3 = LED 4. rumbleMs 0: none (the DS3 counts ~20 ms units). False when
// the report wasn't sent (no memory, or the stack refused it).
bool send(uint8_t leds, uint32_t rumbleMs, uint8_t intensity);
}
