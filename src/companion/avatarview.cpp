#include "avatarview.h"
#include <Arduino.h>
#include <cstring>
#include <esp_system.h>
#include "../util/avatarbin.h"
#include "../util/avatarscene.h"
#include "../util/avatarsing.h"

extern const uint8_t _binary_data_avatar_bin_start[] asm("_binary_data_avatar_bin_start");
extern const uint8_t _binary_data_avatar_bin_end[] asm("_binary_data_avatar_bin_end");

namespace avatarview {
static constexpr uint16_t TAG_PINK = 0xF497;   // #F090B8
static avatarbin::Info s_info{};
static int s_x = 0, s_y = 0, s_scale = 0;        // where the current screen shows her
static int s_entry = -1, s_frame = -1;           // on screen (-1: repaint)
static bool s_tag = false;
static bool s_talking = false;                    // her own line is up
static uint32_t s_t0 = 0;
static int s_lineEntry = 0;
static char s_text[avatarlines::TEXT_CAP], s_typed[avatarlines::TEXT_CAP];

bool begin() {
    const char* why = "";
    const size_t len = (size_t)(_binary_data_avatar_bin_end - _binary_data_avatar_bin_start);
    if (avatarbin::parse(_binary_data_avatar_bin_start, len, s_info, &why)) {
        Serial.printf("[avatar] %s: %d moods\n", s_info.name, s_info.moods);
        return true;
    }
    Serial.printf("[avatar] off: %s\n", why);
    return false;
}

bool on() { return s_info.moods > 0; }
void invalidate() { s_entry = s_frame = -1; s_tag = false; }

void place(int x, int y, int scale) {
    if (x == s_x && y == s_y && scale == s_scale) return;
    s_x = x;
    s_y = y;
    s_scale = scale;
    invalidate();
}

// Frame f of entry e at (s_x, s_y), 1x or pixel-doubled 2x, row by row through a RAM buffer
// (the embedded file need not be 2-byte aligned); then her name tag, once.
static void show(TFT_eSPI& t, int e, int f) {
    constexpr int N = avatarbin::SIDE;
    if (s_scale == 0 || (e == s_entry && f == s_frame)) return;
    const uint8_t* px = _binary_data_avatar_bin_start + avatarbin::frameOffset(s_info, e, f);
    static uint16_t src[N], row[2 * N];
    for (int y = 0; y < N; ++y) {
        memcpy(src, px + (size_t)y * N * 2, N * 2);   // RGB565 high byte first, pushed as stored
        if (s_scale == 1) {
            t.pushImage(s_x, s_y + y, N, 1, src);
            continue;
        }
        for (int x = 0; x < N; ++x) row[2 * x] = row[2 * x + 1] = src[x];
        t.pushImage(s_x, s_y + 2 * y, 2 * N, 1, row);
        t.pushImage(s_x, s_y + 2 * y + 1, 2 * N, 1, row);
    }
    s_entry = e;
    s_frame = f;
    if (!s_tag) {                                 // the name tag, centred under her
        const int side = N * s_scale;
        const int w = (int)strlen(s_info.name) * 6 + 6;
        const int x = s_x + (side - w) / 2;
        t.fillRect(x, s_y + side + 2, w, 9, TAG_PINK);
        t.setTextDatum(TL_DATUM);
        t.setTextColor(TFT_WHITE, TAG_PINK);
        t.drawString(s_info.name, x + 3, s_y + side + 3, 1);
        s_tag = true;
    }
}

void say(avatarlines::Event ev, const decklink::View& v, uint32_t now) {
    static bool spoke = false;
    static uint32_t lastAt = 0;
    static avatarlines::Out out;
    if (!on()) return;
    const avatarlines::In in{ev, spoke, now - lastAt, esp_random(), v.deck.track, v.deck.artist, ""};
    if (!avatarlines::pick(in, out)) return;
    spoke = true;
    lastAt = now;
    s_lineEntry = avatarbin::find(s_info, out.mood);
    strlcpy(s_text, out.text, sizeof s_text);
    s_t0 = now;
    s_talking = true;
    Serial.printf("[avatar] says: %s\n", out.text);
}

const char* update(TFT_eSPI& t, const decklink::View& v, uint32_t now) {
    if (!on()) return nullptr;
    static bool greeted = false;
    static uint32_t gen = 0;
    if (v.polledAtMs && v.deckOk) {              // events come from the new deck's feed
        if (!greeted) {
            greeted = true;
            gen = v.deck.trackGen;
            say(avatarlines::Event::Boot, v, now);
        } else if (v.deck.trackGen != gen) {
            gen = v.deck.trackGen;
            say(avatarlines::Event::Track, v, now);
        }
    }
    if (s_talking) {                             // her line: typing + hold
        const avatarscene::State st = avatarscene::at(now - s_t0 + avatarscene::SLIDE_MS, s_text,
                                                      s_info.frames[s_lineEntry]);
        if (st.phase != avatarscene::Phase::Done) {
            show(t, s_lineEntry, st.frame);
            memcpy(s_typed, s_text, st.bytes);
            s_typed[st.bytes] = '\0';
            return s_typed;
        }
        s_talking = false;
    }
    const stateparse::Deck& d = v.deck;
    const int talk = avatarbin::find(s_info, avatarbin::Talk);
    const bool synced = strcmp(d.lyricKind, "synced") == 0;
    if (v.polledAtMs && d.playing && synced && d.line[0] && s_info.id[talk] == avatarbin::Talk) {
        const uint32_t pos = d.progressMs + (now - v.polledAtMs);
        uint32_t start = d.lineStartMs, next = d.nextStartMs;
        if (next && pos >= next) {               // the next line is already on screen
            start = next;
            next = 0;
        }
        show(t, talk, avatarsing::frame(pos, start, next, s_info.frames[talk]));
    } else {
        show(t, avatarbin::find(s_info, avatarbin::Smug), 0);
    }
    return nullptr;
}
}
