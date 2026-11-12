#include "irrx.h"

#include <Arduino.h>

#define DECODE_NEC  // only the light kit's protocol: less flash, and noise doesn't decode as LG or the like
#include <IRremote.hpp>

namespace irrx {
void begin(int pin) { IrReceiver.begin(pin, DISABLE_LED_FEEDBACK); }

bool poll(uint16_t* address, uint8_t* command, bool* repeat) {
    if (!IrReceiver.decode()) return false;
    const IRData& d = IrReceiver.decodedIRData;
    const bool ok = d.protocol == NEC;
    if (ok) {
        *address = d.address;
        *command = static_cast<uint8_t>(d.command);
        *repeat = (d.flags & IRDATA_FLAGS_IS_REPEAT) != 0;
    }
    IrReceiver.resume();
    return ok;
}
}  // namespace irrx
