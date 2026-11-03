#include "camband.h"
#include <cstdio>
#include <cstring>
#include "../../util/common/rgb565.h"
#include "../../util/common/text.h"

namespace camband {

using cameras::Stage;
using rgb565::rgb;

// Our call (the spec fixes only "red for an alarm"): a dark band with the kind's colour along its
// top for an alert (orange for a red light, as on the yellow deck), solid red for an alarm. The same
// day and night: it must read at a glance in sunlight.
static constexpr uint16_t ALERT_BG = rgb(0x10, 0x14, 0x18);
static constexpr uint16_t ALARM_BG = rgb(0xD0, 0x10, 0x10);
static constexpr uint16_t YELLOW = rgb(0xF8, 0xC8, 0x00);
static constexpr uint16_t ORANGE = rgb(0xF8, 0x80, 0x00);
static constexpr uint16_t SIGN_RED = rgb(0xE0, 0x18, 0x18);
static constexpr uint16_t LAMP_OFF = rgb(0x40, 0x40, 0x40);
static constexpr uint16_t LABEL_GREY = rgb(0xC8, 0xC8, 0xC8);
static constexpr int ACCENT_H = 3;
static constexpr int PAD = 6;                 // right margin
static constexpr int TEXT_X = 52;             // right of the icon
static constexpr int DIST_W = 84, KMH_W = 74; // the number boxes, right-aligned

struct Shown {
    bool on = false;          // something of ours is on screen (and matches the fields below)
    Stage stage = Stage::Off;
    char id[sizeof camlist::Point::id] = "";
    uint16_t dist = 0, kmh = 0;
};
static Shown s_shown;

int rows(const cameras::Status& s) {
    switch (s.stage) {
    case Stage::Alert:
    case Stage::Alarm: return BAND_H;
    case Stage::NoFix:   // a corner tag over the screen, drawn after it (TAG)
    case Stage::Off: break;
    }
    return 0;
}

void invalidate() { s_shown.on = false; }

static void icon(TFT_eSPI& tft, camlist::Kind k, uint8_t limit, int cx, int cy) {
    switch (k) {
    case camlist::Kind::Speed:   // the round limit sign
        tft.fillCircle(cx, cy, 19, TFT_WHITE);
        tft.fillCircle(cx, cy, 17, SIGN_RED);
        tft.fillCircle(cx, cy, 12, TFT_WHITE);
        if (limit != camlist::NO_LIMIT) {
            char n[4];
            snprintf(n, sizeof n, "%u", (unsigned)limit);
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_BLACK, TFT_WHITE);
            tft.drawString(n, cx, cy + 1, 2);
        }
        break;
    case camlist::Kind::Bump: {  // Brazil's yellow diamond warning sign, a hump on a line
        tft.fillTriangle(cx, cy - 19, cx - 19, cy, cx + 19, cy, YELLOW);
        tft.fillTriangle(cx, cy + 19, cx - 19, cy, cx + 19, cy, YELLOW);
        tft.drawLine(cx, cy - 19, cx + 19, cy, TFT_BLACK);
        tft.drawLine(cx + 19, cy, cx, cy + 19, TFT_BLACK);
        tft.drawLine(cx, cy + 19, cx - 19, cy, TFT_BLACK);
        tft.drawLine(cx - 19, cy, cx, cy - 19, TFT_BLACK);
        tft.fillCircle(cx, cy + 5, 7, TFT_BLACK);
        tft.fillRect(cx - 8, cy + 6, 17, 8, YELLOW);
        tft.fillRect(cx - 11, cy + 6, 23, 2, TFT_BLACK);
        break;
    }
    case camlist::Kind::RedLight:  // a traffic light on red
        tft.fillRoundRect(cx - 9, cy - 19, 18, 38, 4, TFT_BLACK);
        tft.drawRoundRect(cx - 9, cy - 19, 18, 38, 4, TFT_WHITE);
        tft.fillCircle(cx, cy - 11, 5, SIGN_RED);
        tft.fillCircle(cx, cy, 5, LAMP_OFF);
        tft.fillCircle(cx, cy + 11, 5, LAMP_OFF);
        break;
    }
}

static void numbers(TFT_eSPI& tft, int y0, uint16_t bg, uint16_t dist, uint16_t kmh) {
    const int right = tft.width() - PAD;
    char t[16];
    tft.fillRect(right - DIST_W, y0 + ACCENT_H, DIST_W, 25, bg);
    tft.fillRect(right - KMH_W, y0 + 29, KMH_W, 18, bg);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(TFT_WHITE, bg);
    snprintf(t, sizeof t, "%u m", (unsigned)dist);
    tft.drawString(t, right, y0 + 5, 4);
    snprintf(t, sizeof t, "%u km/h", (unsigned)kmh);
    tft.drawString(t, right, y0 + 30, 2);
}

static void band(TFT_eSPI& tft, const cameras::Status& s, const camlist::Point* p, uint16_t dist) {
    const int w = tft.width(), y0 = tft.height() - BAND_H;
    const bool alarm = s.stage == Stage::Alarm;
    const uint16_t bg = alarm ? ALARM_BG : ALERT_BG;
    const camlist::Kind kind = p ? p->kind : camlist::Kind::Speed;
    const uint8_t limit = p ? p->limit : camlist::NO_LIMIT;
    tft.fillRect(0, y0, w, BAND_H, bg);
    tft.fillRect(0, y0, w, ACCENT_H, alarm ? TFT_WHITE : kind == camlist::Kind::RedLight ? ORANGE : YELLOW);
    icon(tft, kind, limit, 24, y0 + 3 + (BAND_H - 3) / 2);
    const char* name = kind == camlist::Kind::Bump ? "LOMBADA" : kind == camlist::Kind::RedLight ? "SINAL" : "RADAR";
    char t[32];
    if (limit != camlist::NO_LIMIT && kind != camlist::Kind::RedLight)
        snprintf(t, sizeof t, "%s %u", name, (unsigned)limit);
    else
        snprintf(t, sizeof t, "%s", name);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TFT_WHITE, bg);
    tft.drawString(t, TEXT_X, y0 + 5, 4);
    if (p && p->label[0]) {   // the built-in font is ASCII: accents folded, cut to its room
        txt::asciiFold(p->label, t, sizeof t);
        const int room = w - PAD - KMH_W - 4 - TEXT_X;
        for (size_t n = strlen(t); n > 0 && tft.textWidth(t, 2) > room; --n) t[n - 1] = '\0';
        tft.setTextColor(alarm ? TFT_WHITE : LABEL_GREY, bg);
        tft.drawString(t, TEXT_X, y0 + 30, 2);
    }
    numbers(tft, y0, bg, dist, s.kmh);
}

static void tag(TFT_eSPI& tft) {
    int w = tft.textWidth("GPS --", 2) + 8;
    if (w > TAG.w) w = TAG.w;
    tft.fillRoundRect(TAG.x, TAG.y, w, TAG.h, 3, TFT_BLACK);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(ORANGE, TFT_BLACK);
    tft.drawString("GPS --", TAG.x + 4, TAG.y + TAG.h / 2, 2);
}

void draw(TFT_eSPI& tft, const cameras::Status& s, const camlist::Point* active) {
    if (s.stage == Stage::Off) {
        s_shown.on = false;
        s_shown.stage = Stage::Off;
        return;
    }
    const char* id = active ? active->id : "";
    // 10 m steps: one redraw per ~10 m of countdown at most, the fix rate's anyway.
    const uint16_t dist = (uint16_t)((s.distM + 5) / 10 * 10);
    const bool whole = !s_shown.on || s.stage != s_shown.stage || strcmp(id, s_shown.id) != 0;
    if (!whole && (s.stage == Stage::NoFix || (dist == s_shown.dist && s.kmh == s_shown.kmh))) return;
    if (s.stage == Stage::NoFix) tag(tft);
    else if (whole) band(tft, s, active, dist);
    else numbers(tft, tft.height() - BAND_H, s.stage == Stage::Alarm ? ALARM_BG : ALERT_BG, dist, s.kmh);
    tft.setTextDatum(TL_DATUM);   // the screens draw with the default datum
    s_shown.on = true;
    s_shown.stage = s.stage;
    strncpy(s_shown.id, id, sizeof s_shown.id - 1);
    s_shown.id[sizeof s_shown.id - 1] = '\0';
    s_shown.dist = dist;
    s_shown.kmh = s.kmh;
}

}  // namespace camband
