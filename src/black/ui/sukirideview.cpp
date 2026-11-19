#include "sukirideview.h"
#include <Arduino.h>
#include "../../common/core/embed.h"
#include "../../util/car/drivedash.h"
#include "../../util/car/sukiride.h"
#include "../core/mem.h"
EMBED(data_sukiride_bin);

namespace ui {
namespace {
using sukiride::Rect;
int s_state = 0;   // 0 not read yet, 1 ready, -1 no art (or no buffer)
sukiride::Art s_art;
sukiride::Look s_look;
uint16_t* s_buf = nullptr;   // one region's pixels (PSRAM)
sukiride::Trace s_trace;
drivedash::Alerts s_alerts;
uint32_t s_key[sukiride::REGIONS];
int s_theme = -2;
bool s_night = false, s_live = false, s_demo = false;

void push(TFT_eSPI& tft, const sukiride::Scene& sc, Rect r) {
    sukiride::compose(s_art, s_look, sc, r, s_buf);
    tft.pushImage(r.x, r.y, r.w, r.h, s_buf);
}
}  // namespace

bool rideReady() {
    if (s_state == 0) {
        s_state = -1;
        if (sukiride::parse(EMBED_DATA(data_sukiride_bin), EMBED_SIZE(data_sukiride_bin), &s_art)) {
            s_buf = (uint16_t*)mem::bigAlloc(sukiride::MAX_PIXELS * sizeof(uint16_t));
            if (s_buf) s_state = 1;
        }
        Serial.printf("[screen] ride art %s\n", s_state == 1 ? "found" : "missing: another screen");
    }
    return s_state == 1;
}

void drawRide(TFT_eSPI& tft, const obdwire::Live& l, uint32_t nowMs, int theme, bool night, bool demo, bool whole) {
    if (!rideReady()) return;
    if (theme != s_theme || night != s_night) {   // a new colour: the palette again, and all of it
        sukiride::makeLook(s_art, theme, night, &s_look);
        s_theme = theme;
        s_night = night;
        whole = true;
    }
    const bool live = l.link == obdplan::Link::Live;
    if (live != s_live) s_trace.reset();   // a new link: a new trace
    if (live != s_live || demo != s_demo) whole = true;   // the whole screen dims (or comes back); the state tag
    s_live = live;
    s_demo = demo;
    s_trace.update(live ? l.rpm : NAN, nowMs);
    s_alerts.update(live ? l.coolC : NAN, live ? l.volts : NAN, live ? l.rpm : NAN, nowMs);
    sukiride::Scene sc;
    sukiride::scene(l, s_trace, s_alerts.coolant(), s_alerts.volts(), nowMs, &sc);
    sc.demo = demo;
    if (whole) {
        for (int y = 0; y < sukiride::H; y += sukiride::STRIP_H) {
            const int h = y + sukiride::STRIP_H > sukiride::H ? sukiride::H - y : sukiride::STRIP_H;
            push(tft, sc, Rect{0, (int16_t)y, (int16_t)sukiride::W, (int16_t)h});
        }
        for (int r = 0; r < sukiride::REGIONS; ++r) s_key[r] = sukiride::key(sc, (sukiride::Region)r);
        return;
    }
    for (int r = 0; r < sukiride::REGIONS; ++r) {
        const uint32_t k = sukiride::key(sc, (sukiride::Region)r);
        if (k == s_key[r]) continue;
        s_key[r] = k;
        push(tft, sc, sukiride::region((sukiride::Region)r));
    }
}
}  // namespace ui
