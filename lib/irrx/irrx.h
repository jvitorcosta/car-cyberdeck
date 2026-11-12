#pragma once
// The yellow deck's IR receiver (a KY-022 on CN1, src/util/yellow/irkeys for what the codes mean).
// IRremote is header-only and #warnings on purpose, so it can't compile under src/'s -Werror: this
// little library under lib/ is the only code that includes it. NEC only (the light kit's remote):
// other protocols aren't decoded, so stray frames of noise don't come through.
#include <cstdint>

namespace irrx {
// Starts receiving on `pin` (IRremote samples it from a timer interrupt).
void begin(int pin);
// True once per decoded NEC frame: its address and command, and whether it is a repeat frame (a
// held button). The receiver is ready for the next frame when it returns.
bool poll(uint16_t* address, uint8_t* command, bool* repeat);
}  // namespace irrx
