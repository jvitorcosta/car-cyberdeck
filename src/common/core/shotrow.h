#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

namespace shotrow {
// One panel row on the serial port as "R<y> <320 x 4 hex digits>" and a newline (RGB565,
// big-endian), for tools/deck_shot.py. `late`: the bytes this panel's read comes behind (a dummy: 1 on the CYD, 0 on
// the S3's). False only when the port took less than the whole row; no memory for it skips the row.
inline bool send(TFT_eSPI& tft, int y, size_t late) {
    static const char HEX_DIGITS[] = "0123456789abcdef";
    const size_t rgbLen = 320 * 3 + late;
    constexpr size_t ROW_LEN = 6 + 320 * 4 + 2;
    auto* rgb = static_cast<uint8_t*>(malloc(rgbLen + ROW_LEN));
    if (!rgb) return true;
    char* row = reinterpret_cast<char*>(rgb + rgbLen);
    tft.readRectRGB(0, y, 320, 1, rgb);
    int n = snprintf(row, ROW_LEN, "R%03d ", y);
    for (int x = 0; x < 320; x++) {
        const uint8_t* p = rgb + x * 3 + late;
        const unsigned v = ((p[0] & 0xF8U) << 8) | ((p[1] & 0xFCU) << 3) | (p[2] >> 3);
        for (int s = 12; s >= 0; s -= 4) row[n++] = HEX_DIGITS[(v >> s) & 15U];
    }
    row[n++] = '\n';
    const size_t sent = Serial.write(reinterpret_cast<const uint8_t*>(row), static_cast<size_t>(n));
    free(rgb);
    return sent == static_cast<size_t>(n);
}
}  // namespace shotrow
