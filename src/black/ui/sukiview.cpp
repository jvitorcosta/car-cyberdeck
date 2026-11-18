#include "sukiview.h"
#include <Arduino.h>
#include <cmath>
#include <cstring>
#include <ctime>
#include "../../util/car/gaugehud.h"
#include "../../util/car/sprint.h"
#include "../../util/car/streetface.h"
#include "../../util/car/sukihud.h"
#include "../../util/common/rgb565.h"
#include "../core/media.h"
#include "../core/mem.h"
MEDIA_DECL(data_suki_bin);

namespace ui {
namespace {
using sukihud::Align;
using sukihud::Rect;
using rgb565::be;
using rgb565::rgb;

// big-endian, like the buffer
constexpr uint16_t PINK = be(rgb(255, 40, 112)), PINK_DIM = be(rgb(70, 10, 34)), RED = be(rgb(236, 16, 24));
constexpr uint16_t CORE = be(rgb(255, 235, 245)), WAVE_DOT = be(rgb(120, 20, 60));
constexpr uint16_t TAG = rgb(236, 16, 24), TAG_BG = rgb(14, 0, 6);   // native: drawString

int s_state = 0;   // 0 not read yet, 1 the art is here, -1 it isn't
sukihud::Art s_art;
uint16_t* s_buf = nullptr;   // one region's pixels (PSRAM)
sprint::Timer s_sprint;
streetface::Trace s_trace;
struct Shown {
    char bat[8], thr[8], h2o[8], iat[8], clock[6], st3[12], st2[12], st1[8], speed[4], rpm[8], load[6];
    int tach, loadH;
    bool loadHot;
    obdplan::Link link;
    uint32_t waveTick;
} s_shown;

void push(TFT_eSPI& tft, Rect r) { tft.pushImage(r.x, r.y, r.w, r.h, s_buf); }

// A text region: the background, then s; pushed only when s changed (or whole).
void textRegion(TFT_eSPI& tft, Rect r, sukihud::FontId f, int x, Align al, const char* s, char* shown,
                size_t n, bool whole) {
    if (!whole && strcmp(s, shown) == 0) return;
    sukihud::background(s_art, r, s_buf);
    sukihud::text(s_art, f, r, s_buf, x, r.y + r.h / 2, s, al);
    push(tft, r);
    strlcpy(shown, s, n);
}

void clockText(char out[6]) {
    const time_t now = time(nullptr);
    struct tm lt;
    if (now < 1600000000 || !localtime_r(&now, &lt)) {   // not synced yet
        strcpy(out, "--:--");
        return;
    }
    const int h = lt.tm_hour % 24, m = lt.tm_min % 60;   // by hand: -Werror=format-truncation
    out[0] = (char)('0' + h / 10);
    out[1] = (char)('0' + h % 10);
    out[2] = ':';
    out[3] = (char)('0' + m / 10);
    out[4] = (char)('0' + m % 10);
    out[5] = 0;
}

// v as text with `decimals`, then a suffix ('*' is the font's degree sign); "--" when unknown.
void value(float v, int decimals, const char* suffix, char* out, size_t n) {
    gaugehud::text(v, decimals, out, n);
    if (std::isfinite(v)) strlcat(out, suffix, n);
}

void wave(TFT_eSPI& tft) {
    const Rect r = sukihud::WAVE;
    sukihud::background(s_art, r, s_buf);
    const int mid = r.y + r.h / 2, hMax = r.h / 2 - 2;
    for (int i = 0; i < streetface::Trace::N; ++i) {
        const int cx = r.x + 3 + (i * (r.w - 7)) / (streetface::Trace::N - 1);
        const int h = s_trace.height(i, hMax);
        if (h < 0) sukihud::capsule(r, s_buf, cx, mid - 1, mid + 1, 3, WAVE_DOT, 0);
        else sukihud::capsule(r, s_buf, cx, mid - h - 2, mid + h + 2, 3, PINK, CORE);
    }
    push(tft, r);
}

void tach(TFT_eSPI& tft, int lit) {
    const Rect r = sukihud::TACH;
    sukihud::background(s_art, r, s_buf);
    for (int i = 0; i < sukihud::TACH_N; ++i) {
        const bool on = i < lit;
        const uint16_t rim = on ? (sukihud::tachHot(i) ? RED : PINK) : PINK_DIM;
        sukihud::capsule(r, s_buf, sukihud::tachX(i), sukihud::tachTop(i), sukihud::TACH_BOTTOM, 5, rim, on ? CORE : 0);
    }
    push(tft, r);
}

void load(TFT_eSPI& tft, int h, bool hot, const char* pct) {
    const Rect r = sukihud::LOAD;
    sukihud::background(s_art, r, s_buf);
    const int cx = sukihud::LOAD_CX, bot = sukihud::LOAD_BOTTOM, w = sukihud::LOAD_W;
    sukihud::capsule(r, s_buf, cx, bot - sukihud::LOAD_H, bot, w, PINK_DIM, 0);
    if (h > 0) sukihud::capsule(r, s_buf, cx, bot - h, bot, h < w ? h : w, hot ? RED : PINK, CORE);
    sukihud::text(s_art, sukihud::F_LOAD, r, s_buf, cx, 232, pct, Align::Center);
    push(tft, r);
}
}  // namespace

bool sukiReady() {
    if (s_state == 0) {
        const media::Blob b = MEDIA(data_suki_bin, "suki.bin");
        s_state = -1;
        if (sukihud::parse(b.data, b.size, &s_art)) {
            s_buf = (uint16_t*)mem::bigAlloc(sukihud::MAX_PIXELS * sizeof(uint16_t));
            if (s_buf) s_state = 1;
        }
        Serial.printf("[screen] suki art %s\n", s_state == 1 ? "found" : "missing: street look");
    }
    return s_state == 1;
}

void drawSuki(TFT_eSPI& tft, const obdwire::Live& l, uint32_t nowMs, bool whole) {
    if (!sukiReady()) return;
    if (whole) {
        tft.pushImage(0, 0, sukihud::W, sukihud::H, (const uint16_t*)s_art.bg);
        memset(&s_shown, 0, sizeof s_shown);
        s_shown.tach = s_shown.loadH = -1;
        s_trace.reset();
    }
    const bool live = l.link == obdplan::Link::Live;
    s_sprint.update(live ? l.kmh : NAN, nowMs);
    s_trace.update(live ? l.rpm : NAN, nowMs);
    // the waveform: when the trace took a sample
    if (whole || s_trace.samples() != s_shown.waveTick) {
        wave(tft);
        s_shown.waveTick = s_trace.samples();
    }
    char t[12];
    // the FD / HD lines
    value(l.volts, 1, "", t, sizeof t);
    textRegion(tft, sukihud::BAT, sukihud::F_SMALL, sukihud::BAT.x, Align::Left, t, s_shown.bat, sizeof s_shown.bat, whole);
    value(l.throttle, 0, "%", t, sizeof t);
    textRegion(tft, sukihud::THR, sukihud::F_SMALL, sukihud::THR.x, Align::Left, t, s_shown.thr, sizeof s_shown.thr, whole);
    value(l.coolC, 0, "*", t, sizeof t);
    textRegion(tft, sukihud::H2O, sukihud::F_SMALL, sukihud::H2O.x, Align::Left, t, s_shown.h2o, sizeof s_shown.h2o, whole);
    value(l.intakeC, 0, "*", t, sizeof t);
    textRegion(tft, sukihud::IAT, sukihud::F_SMALL, sukihud::IAT.x, Align::Left, t, s_shown.iat, sizeof s_shown.iat, whole);
    // the clock
    clockText(t);
    textRegion(tft, sukihud::CLOCK, sukihud::F_CLOCK, sukihud::CLOCK.x + sukihud::CLOCK.w - 2, Align::Right, t,
               s_shown.clock, sizeof s_shown.clock, whole);
    // the STAGE bars: the 0-100 (counting while a run is on), the best, km/l
    sprint::text(s_sprint.running() ? s_sprint.elapsed(nowMs) : s_sprint.last(), t, sizeof t);
    textRegion(tft, sukihud::STAGE3, sukihud::F_BAR, sukihud::STAGE3.x + 2, Align::Left, t, s_shown.st3, sizeof s_shown.st3, whole);
    sprint::text(s_sprint.best(), t, sizeof t);
    textRegion(tft, sukihud::STAGE2, sukihud::F_BAR, sukihud::STAGE2.x + 2, Align::Left, t, s_shown.st2, sizeof s_shown.st2, whole);
    value(l.kml, 1, "", t, sizeof t);
    textRegion(tft, sukihud::STAGE1, sukihud::F_BAR, sukihud::STAGE1.x + 2, Align::Left, t, s_shown.st1, sizeof s_shown.st1, whole);
    // the window: speed, tach, rpm (or the link's state), load
    char sp[4];
    gaugehud::speedCells(live ? l.kmh : NAN, sp);
    if (strchr(sp, '-')) strcpy(sp, "--");
    else for (char* c = sp; *c; ++c) if (*c == ' ') *c = '0';   // the film's lap-time look: 087
    textRegion(tft, sukihud::SPEED, sukihud::F_SPEED, sukihud::SPEED.x + 4, Align::Left, sp, s_shown.speed, sizeof s_shown.speed, whole);
    const int lit = live ? sukihud::tachLit(l.rpm) : 0;
    if (whole || lit != s_shown.tach) {
        tach(tft, lit);
        s_shown.tach = lit;
    }
    if (live) {
        gaugehud::text(l.rpm, 0, t, sizeof t);
        textRegion(tft, sukihud::RPM, sukihud::F_RPM, sukihud::RPM.x + 4, Align::Left, t, s_shown.rpm, sizeof s_shown.rpm, whole || s_shown.link != l.link);
    } else if (whole || s_shown.link != l.link) {
        const Rect r = sukihud::RPM;
        sukihud::background(s_art, r, s_buf);
        push(tft, r);
        tft.setTextColor(TAG, TAG_BG);
        tft.setTextDatum(ML_DATUM);
        tft.drawString(gaugehud::linkTag(l.link), r.x + 4, r.y + r.h / 2, 1);
        s_shown.rpm[0] = 0;
    }
    s_shown.link = l.link;
    const int h = live ? sukihud::loadFill(l.load) : 0;
    const bool hot = live && std::isfinite(l.load) && l.load >= sukihud::LOAD_HOT;
    value(live ? l.load : NAN, 0, "%", t, sizeof t);
    if (whole || h != s_shown.loadH || hot != s_shown.loadHot || strcmp(t, s_shown.load) != 0) {
        load(tft, h, hot, t);
        s_shown.loadH = h;
        s_shown.loadHot = hot;
        strlcpy(s_shown.load, t, sizeof s_shown.load);
    }
}
}  // namespace ui
