#include "camband.h"
#include <cstdio>
#include <cstring>
#include "../../util/car/camsign.h"
#include "../../util/common/rgb565.h"

namespace camband {

using cameras::Stage;
using camlist::Kind;
using rgb565::rgb;

// The owner's approved mockup (2026-11-03): a dark band and panel for an alert, the band solid red
// for an alarm; the signs in their road colours. The same day and night: it must read at a glance
// in sunlight.
static constexpr uint16_t ALERT_BG = rgb(0x10, 0x14, 0x18);   // the band's, and the panel's always
static constexpr uint16_t ALARM_BG = rgb(0xD0, 0x10, 0x10);
static constexpr uint16_t YELLOW = rgb(0xF8, 0xC8, 0x00);
static constexpr uint16_t ORANGE = rgb(0xF8, 0x80, 0x00);
static constexpr uint16_t SIGN_RED = rgb(0xD8, 0x18, 0x18);
static constexpr uint16_t LAMP_GREEN = rgb(0x10, 0xC0, 0x40);
static constexpr int BAR_H = 4;                                // the countdown bar, the band's top rows
static constexpr int PAD = 6;                                  // the band's side margins
static constexpr int DIST_R = 140, DIST_UNIT_X = 144;          // "380" right-aligned, then "m"
static constexpr int NUM_BASE = 34;                            // the numbers' baseline, below the band's top
static constexpr int DIST_BOX_X = 70, DIST_BOX_W = 100;        // cleared for a new distance
static constexpr int KMH_BOX_X = 196;                          // cleared for a new speed, to the edge
// The panel: the main sign centred at (SIGN_X, SIGN_Y), what goes under it at (SIGN_X, BELOW_Y).
static constexpr int SIGN_X = PANEL.x + PANEL.w / 2, SIGN_Y = 68;
static constexpr int RIM_R = 56, RING_R = 53, FACE_R = 41;     // R-19: white rim, red ring, white face
static constexpr int DIGITS_Y = 66, KMH_Y = 100;               // the limit's middle, "km/h" under it
static constexpr int BELOW_Y = 156;
static constexpr int BELOW_TOP = 128, BELOW_H = PANEL.y + PANEL.h - BELOW_TOP;   // the +N's room

struct Shown {
    bool on = false;          // something of ours is on screen (and matches the fields below)
    Stage stage = Stage::Off;
    bool mute = false;
    char id[sizeof camlist::Point::id] = "";
    uint16_t dist = 0, kmh = 0;
    int bar = -1;             // the countdown bar's width
    bool red = false;         // the limit sign shows its red face (alarm blink)
    uint16_t over = 0;        // the `+N` shown, 0 = the badge / bump sign
};
static Shown s_shown;
// Kept apart from s_shown: an invalidate() (the screen drew over us) is no new alert, and must not
// refill the bar or restart the blink.
static camsign::Countdown s_bar;
static bool s_barOn = false;
static char s_barId[sizeof camlist::Point::id] = "";
static uint32_t s_alarmSince = 0;

int rows(const cameras::Status& s) {
    switch (s.stage) {
    case Stage::Alert:
    case Stage::Alarm: return BAND_H;
    case Stage::NoFix:   // a corner tag over the screen, drawn after it (TAG)
    case Stage::Off: break;
    }
    return 0;
}

Rect screens(const cameras::Status& s, int w, int h) {
    if (!rows(s)) return {0, 0, w, h};
    return {0, 0, PANEL.x, h - BAND_H};
}

void invalidate() { s_shown.on = false; }

// A square standing on its corner (the warning signs), `r` from the centre to each corner.
static void diamond(TFT_eSPI& tft, int cx, int cy, int r, uint16_t c) {
    tft.fillTriangle(cx, cy - r, cx - r, cy, cx + r, cy, c);
    tft.fillTriangle(cx, cy + r, cx - r, cy, cx + r, cy, c);
}

// R-19. `rim`: the white rim too (a blink flip leaves it). `red`: the alarm's red face, white text.
// Font 7 (7-segment, 32 x 48 per digit) for one or two digits and `--`; three digits don't fit
// the face in it, so 100+ is font 4. Transparent text: the face under it was just painted.
static void limitSign(TFT_eSPI& tft, uint8_t limit, bool red, bool rim) {
    if (rim) tft.fillCircle(SIGN_X, SIGN_Y, RIM_R, TFT_WHITE);
    tft.fillCircle(SIGN_X, SIGN_Y, RING_R, SIGN_RED);
    if (!red) tft.fillCircle(SIGN_X, SIGN_Y, FACE_R, TFT_WHITE);
    const uint16_t ink = red ? TFT_WHITE : TFT_BLACK;
    char n[4] = "--";
    if (limit != camlist::NO_LIMIT) snprintf(n, sizeof n, "%u", (unsigned)limit);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(ink);
    tft.drawString(n, SIGN_X, DIGITS_Y, limit >= 100 ? 4 : 7);
    tft.drawString("km/h", SIGN_X, KMH_Y, 2);
}

// A-14: the yellow diamond (black rim) with a traffic light. Never blinks: a red light never alarms.
static void redLightSign(TFT_eSPI& tft) {
    diamond(tft, SIGN_X, SIGN_Y, 57, TFT_BLACK);
    diamond(tft, SIGN_X, SIGN_Y, 51, YELLOW);   // 6 px less to the corner: a 4 px rim
    tft.fillRoundRect(SIGN_X - 13, SIGN_Y - 34, 26, 68, 5, TFT_BLACK);
    tft.fillCircle(SIGN_X, SIGN_Y - 21, 7, SIGN_RED);
    tft.fillCircle(SIGN_X, SIGN_Y, 7, YELLOW);
    tft.fillCircle(SIGN_X, SIGN_Y + 21, 7, LAMP_GREEN);
}

// The camera badge: a white plate with a black camera (body, viewfinder, white lens).
static void badge(TFT_eSPI& tft) {
    const int cx = SIGN_X, cy = BELOW_Y;
    tft.fillRoundRect(cx - 24, cy - 16, 48, 32, 6, TFT_WHITE);
    tft.fillRect(cx - 10, cy - 11, 8, 4, TFT_BLACK);
    tft.fillRoundRect(cx - 13, cy - 7, 26, 17, 3, TFT_BLACK);
    tft.fillCircle(cx, cy + 1, 5, TFT_WHITE);
}

// A-18: the yellow diamond (34 px square, black rim) with a hump on a line.
static void bumpSign(TFT_eSPI& tft) {
    const int cx = SIGN_X, cy = BELOW_Y;
    diamond(tft, cx, cy, 24, TFT_BLACK);
    diamond(tft, cx, cy, 20, YELLOW);   // a ~3 px rim
    tft.fillCircle(cx, cy + 3, 8, TFT_BLACK);
    tft.fillRect(cx - 9, cy + 4, 19, 9, YELLOW);
    tft.fillRect(cx - 13, cy + 3, 27, 3, TFT_BLACK);
}

// The alarm's `+N`, in red under the sign: a drawn plus (font 7 has none), N in font 7.
static void plusN(TFT_eSPI& tft, uint16_t n) {
    char t[6];
    snprintf(t, sizeof t, "%u", (unsigned)n);
    tft.fillRect(PANEL.x, BELOW_TOP, PANEL.w, BELOW_H, ALERT_BG);
    const int plus = 22, gap = 4, w = plus + gap + tft.textWidth(t, 7);
    const int x0 = SIGN_X - w / 2;
    tft.fillRect(x0, BELOW_Y - 3, plus, 6, SIGN_RED);
    tft.fillRect(x0 + plus / 2 - 3, BELOW_Y - plus / 2, 6, plus, SIGN_RED);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(SIGN_RED);
    tft.drawString(t, x0 + plus + gap, BELOW_Y, 7);
}

static void below(TFT_eSPI& tft, Kind k, uint16_t over) {
    if (over) plusN(tft, over);
    else {
        tft.fillRect(PANEL.x, BELOW_TOP, PANEL.w, BELOW_H, ALERT_BG);
        if (k == Kind::Bump) bumpSign(tft);
        else badge(tft);
    }
}

static void panel(TFT_eSPI& tft, Kind k, uint8_t limit, bool red, uint16_t over) {
    tft.fillRect(PANEL.x, PANEL.y, PANEL.w, PANEL.h, ALERT_BG);
    if (k == Kind::RedLight) redLightSign(tft);
    else limitSign(tft, limit, red, true);
    below(tft, k, over);
}

static uint16_t accent(Kind k, bool alarm) { return alarm ? TFT_WHITE : k == Kind::RedLight ? ORANGE : YELLOW; }

static void bar(TFT_eSPI& tft, int y0, int w, uint16_t c, uint16_t bg) {
    const int full = tft.width();
    if (w > 0) tft.fillRect(0, y0, w, BAR_H, c);
    if (w < full) tft.fillRect(w, y0, full - w, BAR_H, bg);
}

// "380 m" and "52 km/h": the numbers in font 4, the units in font 2, on one baseline.
static void numbers(TFT_eSPI& tft, int y0, uint16_t bg, uint16_t dist, uint16_t kmh) {
    const int w = tft.width(), base = y0 + NUM_BASE;
    char t[8];
    tft.fillRect(DIST_BOX_X, y0 + BAR_H, DIST_BOX_W, BAND_H - BAR_H, bg);
    tft.fillRect(KMH_BOX_X, y0 + BAR_H, w - KMH_BOX_X, BAND_H - BAR_H, bg);
    tft.setTextColor(TFT_WHITE, bg);
    snprintf(t, sizeof t, "%u", (unsigned)dist);
    tft.setTextDatum(R_BASELINE);
    tft.drawString(t, DIST_R, base, 4);
    tft.setTextDatum(L_BASELINE);
    tft.drawString("m", DIST_UNIT_X, base, 2);
    const int unitX = w - PAD - tft.textWidth("km/h", 2);
    tft.drawString("km/h", unitX, base, 2);
    snprintf(t, sizeof t, "%u", (unsigned)kmh);
    tft.setTextDatum(R_BASELINE);
    tft.drawString(t, unitX - 4, base, 4);
}

static void band(TFT_eSPI& tft, bool alarm, Kind k, int barW, uint16_t dist, uint16_t kmh, bool mute) {
    const int w = tft.width(), y0 = tft.height() - BAND_H;
    const uint16_t bg = alarm ? ALARM_BG : ALERT_BG, ac = accent(k, alarm);
    tft.fillRect(0, y0, w, BAND_H, bg);
    bar(tft, y0, barW, ac, bg);
    const char* name = k == Kind::Bump ? "LOMBADA" : k == Kind::RedLight ? "SINAL" : "RADAR";
    const int mid = y0 + BAR_H + (BAND_H - BAR_H) / 2;
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(ac, bg);
    tft.drawString(name, PAD, mute ? mid - 10 : mid, 2);
    if (mute) {   // an inverted tag under the kind word, in its colour
        const int mw = tft.textWidth("MUTE", 2) + 6;
        tft.fillRoundRect(PAD, mid + 2, mw, 16, 3, ac);
        tft.setTextColor(alarm ? ALARM_BG : TFT_BLACK, ac);
        tft.drawString("MUTE", PAD + 3, mid + 10, 2);
    }
    numbers(tft, y0, bg, dist, kmh);
}

static void tag(TFT_eSPI& tft) {
    int w = tft.textWidth("GPS --", 2) + 8;
    if (w > TAG.w) w = TAG.w;
    tft.fillRoundRect(TAG.x, TAG.y, w, TAG.h, 3, TFT_BLACK);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(ORANGE, TFT_BLACK);
    tft.drawString("GPS --", TAG.x + 4, TAG.y + TAG.h / 2, 2);
}

void draw(TFT_eSPI& tft, const cameras::Status& s, const camlist::Point* active, bool mute) {
    if (s.stage == Stage::Off || s.stage == Stage::NoFix) s_barOn = false;   // the next alert is new
    if (s.stage == Stage::Off) {
        s_shown.on = false;
        s_shown.stage = Stage::Off;
        return;
    }
    if (s.stage == Stage::NoFix) {
        if (s_shown.on && s_shown.stage == Stage::NoFix) return;
        tag(tft);
        tft.setTextDatum(TL_DATUM);   // the screens draw with the default datum
        s_shown.on = true;
        s_shown.stage = Stage::NoFix;
        return;
    }
    const char* id = active ? active->id : "";
    const Kind kind = active ? active->kind : Kind::Speed;
    const uint8_t limit = active ? active->limit : camlist::NO_LIMIT;
    const bool alarm = s.stage == Stage::Alarm;
    const uint32_t now = millis();
    if (!s_barOn || strcmp(id, s_barId) != 0) {   // a new alert: the bar full from here
        s_bar.start(kind, s.kmh, tft.width());
        s_barOn = true;
        strncpy(s_barId, id, sizeof s_barId - 1);
        s_barId[sizeof s_barId - 1] = '\0';
    }
    const int barW = s_bar.update(s.distM);
    if (alarm && s_shown.stage != Stage::Alarm) s_alarmSince = now;
    const bool red = alarm && kind != Kind::RedLight && camsign::redFace(s_alarmSince, now);
    const uint16_t over = alarm ? camsign::overBy(s.kmh, limit) : 0;
    // 10 m steps: one redraw per ~10 m of countdown at most, the fix rate's anyway.
    const uint16_t dist = (uint16_t)((s.distM + 5) / 10 * 10);
    const bool whole = !s_shown.on || s.stage != s_shown.stage || strcmp(id, s_shown.id) != 0 ||
                       mute != s_shown.mute;
    if (whole) {
        panel(tft, kind, limit, red, over);
        band(tft, alarm, kind, barW, dist, s.kmh, mute);
    } else {
        const int y0 = tft.height() - BAND_H;
        const uint16_t bg = alarm ? ALARM_BG : ALERT_BG;
        if (red != s_shown.red) limitSign(tft, limit, red, false);   // the blink: the sign only
        if (over != s_shown.over) below(tft, kind, over);
        if (barW != s_shown.bar) bar(tft, y0, barW, accent(kind, alarm), bg);
        if (dist != s_shown.dist || s.kmh != s_shown.kmh) numbers(tft, y0, bg, dist, s.kmh);
        else if (red == s_shown.red && over == s_shown.over && barW == s_shown.bar) return;
    }
    tft.setTextDatum(TL_DATUM);   // the screens draw with the default datum
    s_shown.on = true;
    s_shown.stage = s.stage;
    s_shown.mute = mute;
    strncpy(s_shown.id, id, sizeof s_shown.id - 1);
    s_shown.id[sizeof s_shown.id - 1] = '\0';
    s_shown.dist = dist;
    s_shown.kmh = s.kmh;
    s_shown.bar = barW;
    s_shown.red = red;
    s_shown.over = over;
}

}  // namespace camband
