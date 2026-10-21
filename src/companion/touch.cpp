#include "touch.h"
#include <Arduino.h>
#include <SPI.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"   // the library's TS_Point ctor; src/ builds with -Werror
#include <XPT2046_Touchscreen.h>
#pragma GCC diagnostic pop
#include "pins.h"
#include "../util/touchguard.h"
#include "../util/touchmap.h"

namespace touch {
static constexpr int MIN_Z = 400;                 // lighter readings are noise
// This panel never reads "not touched": at rest it reports a press at raw x=0 (y~4000, z~600),
// while real presses (pen) read x >= ~500. A reading at the x=0 edge counts as released, so the
// stuck-press guard below re-arms instead of locking every later press out.
static constexpr int IDLE_RAW_X = 100;
static SPIClass s_spi(VSPI);                      // the TFT owns HSPI; the SD slot is unused here
static XPT2046_Touchscreen s_ts(TOUCH_CS, TOUCH_IRQ);
static const touchmap::Cal CAL{TOUCH_RAW_X0, TOUCH_RAW_X1, TOUCH_RAW_Y0, TOUCH_RAW_Y1};
static bool s_wasDown = false;
static touchguard::Guard s_guard;              // a faulty panel can report one press forever

void begin() {
    s_spi.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    s_ts.begin(s_spi);
    s_ts.setRotation(1);
}

bool read(int* x, int* y) {
    if (!s_ts.touched()) {
        s_guard.update(false, 0, 0, millis());
        s_wasDown = false;
        return false;
    }
    const TS_Point p = s_ts.getPoint();
    if (p.z < MIN_Z || p.x < IDLE_RAW_X) {
        s_guard.update(false, 0, 0, millis());
        s_wasDown = false;
        return false;
    }
    if (!s_guard.update(true, p.x, p.y, millis())) return false;   // a press stuck in one spot
    touchmap::toScreen(CAL, p.x, p.y, x, y);
    if (!s_wasDown) Serial.printf("[touch] raw x=%d y=%d -> %d,%d\n", p.x, p.y, *x, *y);
    s_wasDown = true;
    return true;
}
}
