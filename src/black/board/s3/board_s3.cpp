#include "../../../common/board/board.h"
#include <Arduino.h>

namespace board {

void begin() {
    // Native USB: a write with nobody reading must give up fast. NOT 0: in core 2.0.17's
    // HWCDC::write a 0 timeout means "wait forever" (its retry counter starts at 0 and wraps on
    // the first decrement), so with the deck on a PC that has the port enumerated but closed,
    // the first log line froze the network task until a terminal opened the port: "No internet",
    // stale polls, all cleared the moment anyone started a serial monitor (2026-10-15).
    Serial.setTxTimeoutMs(1);
    Serial.printf("[board] ESP32-S3 PSRAM %u KB (free %u KB), flash %u MB\n",
                  (unsigned)(ESP.getPsramSize() / 1024), (unsigned)(ESP.getFreePsram() / 1024),
                  (unsigned)(ESP.getFlashChipSize() >> 20));
}

}
