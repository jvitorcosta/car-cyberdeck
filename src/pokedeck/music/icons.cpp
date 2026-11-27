// Copied from black/ui/icons.cpp (the header is the black deck's). The icons are drawn pixel by
// pixel: TFT_eSPI's pushImage isn't virtual, so through a TFT_eSPI& it would miss the frame.
#include "../../black/ui/icons.h"
#include <math.h>
#include "../../black/ui/theme.h"

namespace ui {

void drawIcon(TFT_eSPI& t, icons::Icon i, int x, int y, uint16_t fg, uint16_t bg) {
    for (int py = 0; py < icons::SIZE; ++py)
        for (int px = 0; px < icons::SIZE; ++px) t.drawPixel(x + px, y + py, icons::pixel(i, px, py) ? fg : bg);
}

void drawCd(TFT_eSPI& t, int cx, int cy, int frame, uint16_t bg) {
    t.fillRect(cx - 7, cy - 7, 15, 15, bg);
    t.fillCircle(cx, cy, 6, theme::CD_SILVER);
    t.drawCircle(cx, cy, 6, theme::pal().iconOff);
    const float a = static_cast<float>(frame & 3) * static_cast<float>(M_PI) / 4.0f;
    const int dx = static_cast<int>(lroundf(5 * cosf(a))), dy = static_cast<int>(lroundf(5 * sinf(a)));
    t.drawLine(cx - dx, cy - dy, cx + dx, cy + dy, TFT_WHITE);   // sheen
    t.fillCircle(cx, cy, 2, theme::pal().top);                    // hub
}

}
