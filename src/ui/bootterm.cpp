#include "bootterm.h"
#include <Arduino.h>
#include <WiFi.h>
#include <cstring>
#include "../util/common/rgb565.h"
#include "../audio/greeting.h"

namespace bootterm {
namespace {

using rgb565::hex;

constexpr uint32_t TERM_TTL = 3000;   // a terminal line disappears this long after its last change

// ---- BIOS colours (util/bios) ----
constexpr uint16_t BIOS_GREY = hex(0xAAAAAA), BIOS_GREEN = hex(0x55FF55), BIOS_YELLOW = hex(0xFFFF55);
constexpr uint16_t WHITE = 0xFFFF, UNDERGLOW = hex(0x8C46FF);

uint16_t biosTone(uint8_t tone) {
    switch (tone) {
    case bios::HEADER: return WHITE;
    case bios::OK: return BIOS_GREEN;
    case bios::WARN: return BIOS_YELLOW;
    case bios::PURPLE: return UNDERGLOW;
    default: return BIOS_GREY;
    }
}

constexpr uint16_t TERM_TEXT = hex(0x6FBF7F);
bios::Term s_term;                    // static: keeps ~210 B off the scene task's small stack
int s_termDrawn[bios::TERM_ROWS];     // characters drawn per row; -1 = redraw
uint32_t s_termGen = 0;
bool (*s_online)() = nullptr;
const char* volatile s_caption = "";
volatile uint32_t s_captionGen = 0;
bool s_wifi = false, s_engine = false, s_greet = false, s_done = false, s_parked = false;   // live lines set

// Row i's text at t ("> " is drawn apart): returns the typed characters, *v the value column (-1 none).
int termRow(int i, uint32_t t, char* out, int* v) {
    const bios::TermLine& r = s_term.rows[i];
    *v = bios::termFormat(r, out);
    return t < r.at ? 0 : bios::typed(t - r.at, (int)strlen(r.label), (int)strlen(out));
}

// Draws row i through draw(text, x, colour): label + dots in terminal green, the value in its tone.
template <typename F>
void termText(int i, uint32_t t, F draw) {
    if (i >= s_term.n) return;
    char out[bios::TERM_COLS + 1], part[bios::TERM_COLS + 1];
    int v;
    const int n = termRow(i, t, out, &v);
    draw("> ", X, TERM_TEXT);
    const int split = v < 0 || n < v ? n : v;
    memcpy(part, out, split);
    part[split] = 0;
    draw(part, X + 12, TERM_TEXT);
    if (v >= 0 && n > v) {
        memcpy(part, out + v, n - v);
        part[n - v] = 0;
        draw(part, X + 12 + v * 6, biosTone(s_term.rows[i].tone));
    }
}

}  // namespace

void begin(bool (*online)()) {
    s_online = online;
    s_term = {};
    s_termGen = 0;
    for (int& d : s_termDrawn) d = -1;
    s_wifi = s_engine = s_greet = s_done = s_parked = false;
}

void setCaption(const char* text) {
    s_caption = text;
    s_captionGen = s_captionGen + 1;
}

const char* caption() { return s_caption; }

uint32_t captionGen() { return s_captionGen; }

uint32_t gen() { return s_term.gen; }

// Cruising: rows straight on the display, redrawn only when they change.
void drawDirect(TFT_eSPI& tft, uint32_t t, uint16_t (*bg)(int y)) {
    const bool all = s_termGen != s_term.gen;
    s_termGen = s_term.gen;
    for (int i = 0; i < bios::TERM_ROWS; ++i) {
        char out[bios::TERM_COLS + 1];
        int v;
        const int n = i < s_term.n ? termRow(i, t, out, &v) : 0;
        if (!all && n == s_termDrawn[i]) continue;
        s_termDrawn[i] = n;
        const int y = Y + i * ROW;
        for (int yy = y; yy < y + 8; ++yy) tft.drawFastHLine(0, yy, X + W, bg(yy));
        termText(i, t, [&](const char* text, int x, uint16_t c) {
            tft.setTextColor(c);                  // one argument: transparent background
            tft.drawString(text, x, y, 1);
        });
    }
}

// Fade-in/brake or the video: rows rendered into a sprite so they dim or move with it.
void drawInto(TFT_eSprite& spr, uint32_t t, int x0, int y0) {
    for (int i = 0; i < s_term.n; ++i) {
        const int y = Y + i * ROW;
        if (y >= y0 + spr.height() || y + 8 <= y0) continue;
        termText(i, t, [&](const char* text, int x, uint16_t c) {
            spr.setTextColor(c);
            spr.drawString(text, x - x0, y - y0, 1);
        });
    }
}

// What the terminal says at t (ms since the scene started): the BIOS script, then the drive's own
// events. Lines scroll up and expire; WI-FI stays while it is still connecting.
void events(uint32_t t, bool braking) {
    bios::termScript(s_term, t);
    if (t >= bios::liveAt() && !s_wifi) {
        if (s_online && s_online()) {
            const String ssid = WiFi.SSID();             // once: no String churn every frame
            char v[bios::TERM_VAL];
            const size_t k = ssid.length() < bios::TERM_VAL - 6 ? ssid.length() : bios::TERM_VAL - 6;
            memcpy(v, ssid.c_str(), k);
            memcpy(v + k, " [OK]", 6);
            bios::termSet(s_term, "WI-FI", v, bios::OK, t);
            s_wifi = true;
        } else {
            // main.cpp's caption mentions "retrying" once every network has failed (connectAny)
            bios::termSet(s_term, "WI-FI", strstr(s_caption, "retry") ? "RETRYING..." : "CONNECTING...",
                          bios::WARN, t, true);
        }
    }
    if (t >= 4800 && !s_engine) s_engine = bios::termSet(s_term, "ENGINE", "RUNNING", bios::OK, t);
    if (t >= 5300 && !s_greet && t < greeting::durationMs())
        s_greet = bios::termSet(s_term, "GREETING", "PLAYING", bios::PURPLE, t);
    if (s_greet && !s_done && t >= greeting::durationMs()) s_done = bios::termSet(s_term, "GREETING", "DONE", bios::OK, t);
    if (braking && !s_parked) s_parked = bios::termSet(s_term, "PARKING", "BRAKE", bios::OK, t);
    bios::termExpire(s_term, t, TERM_TTL);
}

}
