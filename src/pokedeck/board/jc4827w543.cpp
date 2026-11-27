// Guition JC4827W543 (ESP32-S3, 4 MB flash, 8 MB PSRAM), the Pokédeck's board. Display pins from
// moononournation/Dev_Device_Pins (PINS_JC4827W543.h), checked on the board with hwcheck_panel
// (pull request #116), where the touch layer also read half a turn from the picture.
#include <Arduino.h>
#include "panelgfx.h"
#include "../../common/board/board.h"

namespace board {
void begin() {
    Serial.setTxTimeoutMs(1);   // native USB: a PC port left closed must never stall the deck
    Serial.printf("[board] jc4827w543: psram %u KB, flash %u MB\n",
                  static_cast<unsigned>(ESP.getPsramSize() / 1024),
                  static_cast<unsigned>(ESP.getFlashChipSize() >> 20));
}
}  // namespace board

namespace panel {
Arduino_GFX& gfx() {
    static Arduino_ESP32QSPI bus(45 /* cs */, 47 /* sck */, 21, 48, 40, 39 /* d0-d3 */);
    // "IPS" off: on (the pin file's value) shows every colour inverted on this panel.
    static Arduino_NV3041A g(&bus, GFX_NOT_DEFINED, 0, false);
    return g;
}
bool begin() { return gfx().begin(32000000); }
void push(uint16_t* bigEndian) { gfx().draw16bitBeRGBBitmap(0, 0, bigEndian, W, H); }
}  // namespace panel
