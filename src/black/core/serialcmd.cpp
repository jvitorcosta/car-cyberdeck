#include "serialcmd.h"
#include <Arduino.h>
#include <cstdlib>
#include <cstring>
#include "../car/gauges.h"

namespace serialcmd {
namespace {
// One panel row as "R<y> <320 x 4 hex digits>\n" (RGB565, big-endian).
bool shotRow(TFT_eSPI& tft, int y) {
    static const char HEX_DIGITS[] = "0123456789abcdef";
    constexpr size_t RGB_LEN = 320 * 3;   // no dummy byte on this panel (the CYD's has one)
    constexpr size_t ROW_LEN = 6 + 320 * 4 + 2;
    auto* rgb = static_cast<uint8_t*>(malloc(RGB_LEN + ROW_LEN));
    if (!rgb) return true;
    char* row = reinterpret_cast<char*>(rgb + RGB_LEN);
    tft.readRectRGB(0, y, 320, 1, rgb);
    int n = snprintf(row, ROW_LEN, "R%03d ", y);
    for (int x = 0; x < 320; x++) {
        const uint8_t* p = rgb + x * 3;
        const unsigned v = ((p[0] & 0xF8U) << 8) | ((p[1] & 0xFCU) << 3) | (p[2] >> 3);
        for (int s = 12; s >= 0; s -= 4) row[n++] = HEX_DIGITS[(v >> s) & 15U];
    }
    row[n++] = '\n';
    const size_t sent = Serial.write(reinterpret_cast<const uint8_t*>(row), static_cast<size_t>(n));
    free(rgb);
    return sent == static_cast<size_t>(n);
}

void shot(TFT_eSPI& tft) {
    // The USB serial drops what doesn't go out within 1 ms (board_s3: a PC that enumerates the port
    // but doesn't read it must never block a log line). The PC is reading now (pyserial's readline
    // is slow): a write may wait 1 s, and one that still times out ends the shot.
    Serial.setTxTimeoutMs(1000);
    Serial.println("SHOT 320 240");
    for (int y = 0; y < 240; y++)
        if (!shotRow(tft, y)) {
            Serial.setTxTimeoutMs(1);
            return;
        }
    Serial.println("SHOT END");
    char ask[5];
    size_t n = 0;
    for (uint32_t until = millis() + 2000; static_cast<int32_t>(until - millis()) > 0;) {
        if (!Serial.available()) {
            delay(5);
            continue;
        }
        const char c = static_cast<char>(Serial.read());
        if (c == 'Q') break;
        if (c == '\r') continue;
        if (c != '\n') {
            if (n < sizeof ask - 1) ask[n++] = c;
            continue;
        }
        ask[n] = 0;
        n = 0;
        if (ask[0] == 'R' && atoi(ask + 1) < 240) shotRow(tft, atoi(ask + 1));
        until = millis() + 2000;
    }
    Serial.setTxTimeoutMs(1);
}
}  // namespace

void tick(TFT_eSPI& tft) {
    static char line[24];
    static size_t len = 0;
    while (Serial.available()) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r') continue;
        if (c != '\n') {
            if (len < sizeof line - 1) line[len++] = c;
            continue;
        }
        line[len] = 0;
        len = 0;
        if (strcmp(line, "SHOT") == 0) shot(tft);
        else if (strcmp(line, "GAUGES ON") == 0) gauges::set(true);
        else if (strcmp(line, "GAUGES OFF") == 0) gauges::set(false);
        else if (strcmp(line, "GAUGES DEMO") == 0) gauges::demo();
    }
}
}  // namespace serialcmd
