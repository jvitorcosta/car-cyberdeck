#include "screens.h"
#include <cstdio>
#include <cstring>
#include "../ui/battle.h"
#include "../ui/textdraw.h"
#include "../ui/theme.h"

namespace screens {
static char s_bar[160] = "\x01", s_trip[16] = "\x01";

void invalidate() {
    s_bar[0] = '\x01';
    s_trip[0] = '\x01';
}

void button(TFT_eSPI& t, const menunav::Btn& b, bool pressed, float fill, bool cursor) {
    const menunav::Rect& r = b.r;
    const uint16_t face = pressed ? theme::pal().dlgFrame : theme::pal().boxFill;
    t.fillRoundRect(r.x, r.y, r.w, r.h, 6, face);
    if (fill > 0.0f) t.fillRoundRect(r.x, r.y, (int)(r.w * fill), r.h, 6, theme::pal().dlgFrame);
    t.drawRoundRect(r.x, r.y, r.w, r.h, 6, theme::pal().boxBorder);
    const uint16_t fg = (pressed || fill > 0.0f) ? TFT_WHITE : theme::pal().text;
    ui::drawText(t, b.label, r.x + r.w / 2, r.y + r.h / 2, fg, theme::pal().textShadow, MC_DATUM, r.w - 16);
    if (cursor) t.fillTriangle(r.x + 6, r.y + r.h / 2 - 6, r.x + 6, r.y + r.h / 2 + 6, r.x + 14,
                               r.y + r.h / 2, theme::pal().text);
}

void draw(TFT_eSPI& t, menunav::Screen s, bool vroom, menunav::Button cursor) {
    t.fillScreen(theme::pal().top);
    if (s == menunav::Screen::Home) ui::battleBox(t, 200, 8, 112, 162, ui::Tab::None);   // command box
    menunav::Btn b[menunav::MAX_BTNS];
    const int n = menunav::buttons(s, vroom, b);
    for (int i = 0; i < n; ++i)
        button(t, b[i], false, 0.0f, s == menunav::Screen::Home && b[i].id == cursor);
    invalidate();
    bar(t, "");
}

void bar(TFT_eSPI& t, const char* text) {
    const char* s = text ? text : "";
    if (strcmp(s, s_bar) == 0) return;
    strlcpy(s_bar, s, sizeof s_bar);
    const menunav::Rect& r = menunav::BAR;
    ui::dialogueBox(t, r.x, r.y, r.w, r.h);
    if (s[0]) ui::drawText(t, s, r.x + 12, r.y + r.h / 2, theme::pal().text, theme::pal().dlgShadow,
                           ML_DATUM, r.w - 24);
}

void tripTime(TFT_eSPI& t, uint32_t seconds) {
    char txt[16];
    if (seconds >= 3600) snprintf(txt, sizeof txt, "%lu:%02lu", (unsigned long)(seconds / 3600),
                                  (unsigned long)(seconds / 60 % 60));
    else snprintf(txt, sizeof txt, "%lu:%02lu", (unsigned long)(seconds / 60), (unsigned long)(seconds % 60));
    if (strcmp(txt, s_trip) == 0) return;
    strlcpy(s_trip, txt, sizeof s_trip);
    t.fillRect(20, 30, 280, 90, theme::pal().top);
    t.setTextDatum(MC_DATUM);
    t.setTextColor(theme::pal().topText, theme::pal().top);
    t.setTextSize(2);
    t.drawString(txt, 160, 75, 4);
    t.setTextSize(1);
}
}
