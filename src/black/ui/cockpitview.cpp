#include "cockpitview.h"
#include <Arduino.h>
#include <cstring>
#include "../../util/black/lyricface.h"
#include "../../util/car/cockpit.h"
#include "../../util/car/drivedash.h"

namespace ui {
namespace {
using lyricface::Face;
using namespace cockpit;

TFT_eSprite* s_frame = nullptr;   // the screen as drawn (PSRAM)
int s_state = 0;                  // 0 not made yet, 1 ready, -1 no memory
Palette s_pal;
int s_theme = -2;
drivedash::Alerts s_alerts;
uint32_t s_key[REGIONS];
bool s_batch = false;             // a whole redraw: regions are drawn, the frame pushed once at the end

// ---- text: VT323 from the karaoke view's tables, placed by baseline (like ui/textdraw) ----
int width(Face f, const char* s) {
    int w = 0;
    for (; *s; ++s) w += lyricface::advance(f, (uint8_t)*s);
    return w;
}
enum Align : uint8_t { LEFT, RIGHT, CENTER };
int text(Face f, const char* s, int x, int base, uint16_t col, Align al = LEFT) {
    const int w = width(f, s);
    int px = al == RIGHT ? x - w : al == CENTER ? x - w / 2 : x;
    s_frame->setFreeFont(reinterpret_cast<const GFXfont*>(&lyricface::font(f)));
    for (const char* c = s; *c; ++c) {
        s_frame->drawChar(px, base, (uint8_t)*c, col, col, 1);   // bg == fg: drawn transparent
        px += lyricface::advance(f, (uint8_t)*c);
    }
    s_frame->setFreeFont(nullptr);
    return w;
}

void clear(Rect r) { s_frame->fillRect(r.x, r.y, r.w, r.h, s_pal.bg); }   // the panel's black back under r
void push(Rect r) {
    if (!s_batch) s_frame->pushSprite(r.x, r.y, r.x, r.y, r.w, r.h);
}

// ---- the parts ----
void drawFrame() {   // what never changes: the panel and the two labels
    s_frame->fillSprite(s_pal.bg);
    text(Face::LINE, "KM/H", KMH_X, LINE_BASE, s_pal.md);
    text(Face::LINE, "RPM", RPM_LABEL_X, LINE_BASE, s_pal.dm);
}

void drawSpeed(const Scene& s) {
    const Rect r = region(R_SPEED);
    clear(r);
    for (int d = 0; d < SPD_CELLS; ++d) {
        const int x = SPD_X + d * (SPD_CW + SPD_GAP);
        const uint8_t on = drivedash::segments(s.speed[d]);
        for (int k = 0; k < 7; ++k) {
            const drivedash::Box b = drivedash::segment(k, x, SPD_Y, SPD_CW, SPD_CH, SPD_T);
            s_frame->fillRect(b.x, b.y, b.w, b.h, (on >> k) & 1 ? s_pal.tx : s_pal.ghost);
        }
    }
    if (s.status == Status::None) {
        push(r);
        return;
    }
    // CHK shows with a live speed, so its pill must not reach the first cell's right-hand segments (x 49..62 at
    // 100 km/h and up) or the second cell: just "CHK". NO LINK / KEY ON come with "---", whose only lit bar is the middle one.
    const char* t = s.status == Status::Chk ? "CHK" : s.status == Status::KeyOn ? "KEY ON" : "NO LINK";
    const int w = width(Face::LINE, t) + (s.status == Status::Chk ? 12 : 16);
    if (s.status == Status::Chk) {
        s_frame->fillRoundRect(PILL_X, PILL_Y, w, PILL_H, 4, s_pal.al);
        text(Face::LINE, t, PILL_X + w / 2, PILL_Y + 17, s_pal.onAlert, CENTER);
    } else {
        s_frame->fillRoundRect(PILL_X, PILL_Y, w, PILL_H, 4, s_pal.bg);
        s_frame->drawRoundRect(PILL_X, PILL_Y, w, PILL_H, 4, s_pal.al);
        text(Face::LINE, t, PILL_X + w / 2, PILL_Y + 17, s_pal.al, CENTER);
    }
    push(r);
}

void drawRpm(const Scene& s) {
    const Rect r = region(R_RPM);
    clear(r);
    const uint16_t c = s.rpmZone == Zone::Alert ? s_pal.al : s.rpmZone == Zone::Warn ? s_pal.warn : s_pal.tx;
    text(Face::SUNG, s.rpm, RPM_RIGHT, RPM_BASE, c, RIGHT);
    push(r);
}

void drawTach(const Scene& s) {
    const Rect r = region(R_TACH);
    clear(r);
    const int bottom = TACH_Y + TACH_H, hMin = TACH_H * 3 / 10;
    for (int i = 0; i < TACH_N; ++i) {
        const int x0 = TACH_X + i * TACH_W / TACH_N, x1 = TACH_X + (i + 1) * TACH_W / TACH_N - 2;   // 2 px between capsules
        const int w = x1 - x0, h = hMin + ((TACH_H - hMin) * i + (TACH_N - 1) / 2) / (TACH_N - 1);
        const bool on = i < s.tachLit;
        const Zone z = tachZone(i);
        const uint16_t c = !on ? s_pal.ghost : z == Zone::Alert ? s_pal.al : z == Zone::Warn ? s_pal.warn : s_pal.tx;
        s_frame->fillRoundRect(x0, bottom - h, w, h, (w < h ? w : h) / 2, c);
        if (on && h > w + 2) s_frame->drawFastVLine(x0 + w / 2, bottom - h + w / 2, h - w, s_pal.core);
    }
    for (int k = 0; k <= 7; ++k) {   // the scale, x1000 rpm
        int x = TACH_X + TACH_W * k / 7;
        x = x < TACH_X + 4 ? TACH_X + 4 : x > TACH_X + TACH_W - 4 ? TACH_X + TACH_W - 4 : x;
        const char t[2] = {(char)('0' + k), 0};
        text(Face::LINE, t, x, TICK_BASE, s_pal.dm, CENTER);
    }
    push(r);
}

void drawTrip(const Scene& s) {
    const Rect r = region(R_TRIP);
    clear(r);
    const char* const label[2] = {"TRIP", "RANGE"};
    const char* const val[2] = {s.trip, s.range};
    const int x[2] = {TRIP_X0, TRIP_X1};
    for (int i = 0; i < 2; ++i) {
        text(Face::LINE, label[i], x[i], TRIP_LABEL_BASE, s_pal.dm);
        const int w = text(Face::SUNG, val[i], x[i], TRIP_VALUE_BASE, s_pal.tx);
        text(Face::LINE, "km", x[i] + w + 4, TRIP_VALUE_BASE, s_pal.dm);
    }
    push(r);
}

void drawCard(int i, const Scene& s) {
    const Rect r = region((Region)(R_ROW0 + i));
    clear(r);
    const Card& c = s.card[i];
    const uint16_t fill = c.bad ? s_pal.al : s_pal.card, edge = c.bad ? s_pal.al : s_pal.border;
    s_frame->fillRoundRect(r.x, r.y, r.w, r.h, 4, fill);
    s_frame->drawRoundRect(r.x, r.y, r.w, r.h, 4, edge);
    text(Face::LINE, c.label, r.x + 7, r.y + 17, c.bad ? s_pal.onAlert : s_pal.md);
    text(Face::SUNG, c.val, r.x + r.w - 6, r.y + 31, c.bad ? s_pal.onAlert : s_pal.tx, RIGHT);
    if (!c.bad) {   // the bar: PILLS pills over the card's inner width, the danger part red
        const int bx = r.x + 7, bw = r.w - 14, by = r.y + r.h - 8;
        for (int p = 0; p < PILLS; ++p) {
            const int x0 = bx + p * (bw + 2) / PILLS, x1 = bx + (p + 1) * (bw + 2) / PILLS - 2;
            const uint16_t col = p >= c.lit ? s_pal.ghost : p >= c.hot ? s_pal.al : s_pal.tx;
            s_frame->fillRoundRect(x0, by, x1 - x0, 5, 2, col);
        }
    }
    push(r);
}

void drawRegion(Region r, const Scene& s) {
    switch (r) {
        case R_SPEED: drawSpeed(s); break;
        case R_RPM: drawRpm(s); break;
        case R_TACH: drawTach(s); break;
        case R_TRIP: drawTrip(s); break;
        default: drawCard((int)r - (int)R_ROW0, s); break;
    }
}
}  // namespace

bool cockpitReady(TFT_eSPI& tft) {
    if (s_state == 0) {
        s_state = -1;
        s_frame = new TFT_eSprite(&tft);
        s_frame->setColorDepth(16);
        if (s_frame->createSprite(W, H)) {   // PSRAM: TFT_eSprite takes it when there is some
            s_state = 1;
        } else {   // not tried again
            delete s_frame;
            s_frame = nullptr;
        }
        Serial.printf("[screen] cockpit %s\n", s_state == 1 ? "ready" : "out of memory");
    }
    return s_state == 1;
}

void drawCockpit(TFT_eSPI& tft, const obdwire::Live& l, int storedCodes, bool mil, uint32_t nowMs, int theme,
                 bool whole) {
    if (!cockpitReady(tft)) return;
    if (theme != s_theme) {   // a new colour: the palette again, and all of it
        makePalette(theme, &s_pal);
        s_theme = theme;
        whole = true;
    }
    const bool live = l.link == obdplan::Link::Live;
    s_alerts.update(live ? l.coolC : NAN, live ? l.volts : NAN, live ? l.rpm : NAN, nowMs);
    static Scene sc;   // static: off the loop's stack
    scene(l, s_alerts.coolant(), s_alerts.volts(), storedCodes, mil, &sc);
    if (whole) {
        s_batch = true;
        drawFrame();
        for (int r = 0; r < REGIONS; ++r) {
            drawRegion((Region)r, sc);
            s_key[r] = key(sc, (Region)r);
        }
        s_batch = false;
        s_frame->pushSprite(0, 0);
        return;
    }
    for (int r = 0; r < REGIONS; ++r) {
        const uint32_t k = key(sc, (Region)r);
        if (k == s_key[r]) continue;
        s_key[r] = k;
        drawRegion((Region)r, sc);
    }
}
}  // namespace ui
