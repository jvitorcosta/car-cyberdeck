#include "touch_ft6336.h"
#include <Arduino.h>
#include <Wire.h>
#include "pins.h"
#include "../../util/ft6336.h"

namespace touchft {

// Rotation 1 (landscape, USB on the right): the panel's portrait y runs along the screen's x.
// Checked by tapping the corners (the "[touch]" log lines show raw and screen points).
static constexpr ft6336::Orient ORIENT = {true, false, true};
static bool s_ok = false;

static bool readRegs(uint8_t reg, uint8_t* out, int n) {
    Wire.beginTransmission(ft6336::ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)ft6336::ADDR, n) != n) return false;
    for (int i = 0; i < n; ++i) out[i] = (uint8_t)Wire.read();
    return true;
}

bool begin() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);   // also the codec's bus: a no-op when it's up
    uint8_t id = 0;
    s_ok = readRegs(ft6336::REG_CHIP_ID, &id, 1);
    Serial.printf("[touch] FT6336 %s (chip id 0x%02X)\n", s_ok ? "found" : "not found", id);
    return s_ok;
}

bool read(int* x, int* y) {
    if (!s_ok) return false;
    uint8_t r[ft6336::REPORT_LEN];
    if (!readRegs(ft6336::REG_REPORT, r, ft6336::REPORT_LEN)) return false;
    const ft6336::Raw p = ft6336::parse(r);
    if (!p.down) return false;
    ft6336::toScreen(p, ORIENT, x, y);
    static uint32_t lastLog = 0;   // a few lines a second while held: enough to check the mapping
    if (millis() - lastLog > 300) {
        lastLog = millis();
        Serial.printf("[touch] raw %d,%d -> %d,%d\n", p.x, p.y, *x, *y);
    }
    return true;
}

}
