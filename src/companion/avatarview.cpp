#include "avatarview.h"
#include <Arduino.h>
#include <cstring>
#include <esp_system.h>
#include "../ui/theme.h"
#include "../util/avatarbin.h"
#include "../util/avatarlines.h"
#include "../util/avatarscene.h"
#include "../util/avatarsing.h"

extern const uint8_t _binary_data_avatar_bin_start[] asm("_binary_data_avatar_bin_start");
extern const uint8_t _binary_data_avatar_bin_end[] asm("_binary_data_avatar_bin_end");

namespace avatarview {
static constexpr uint16_t TAG_PINK = 0xF497;   // #F090B8
static avatarbin::Info s_info{};
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

// Whole frame, row by row through a RAM buffer (the embedded file need not be 2-byte aligned).
static void show(TFT_eSPI& t, int e, int f) {
    constexpr int N = avatarbin::SIDE;
    if (e == s_entry && f == s_frame) return;
    const uint8_t* px = _binary_data_avatar_bin_start + avatarbin::frameOffset(s_info, e, f);
    static uint16_t row[N];
    for (int y = 0; y < N; ++y) {
        memcpy(row, px + (size_t)y * N * 2, N * 2);
        t.pushImage(X, Y + y, N, 1, row);       // bytes as stored: RGB565 high byte first
    }
    s_entry = e;
    s_frame = f;
    if (!s_tag) {                                // the name tag, centred under her
        const int w = (int)strlen(s_info.name) * 6 + 6;
        const int x = X + (N - w) / 2;
        t.fillRect(x, Y + N + 4, w, 9, TAG_PINK);
        t.setTextDatum(TL_DATUM);
        t.setTextColor(TFT_WHITE, TAG_PINK);
        t.drawString(s_info.name, x + 3, Y + N + 5, 1);
        s_tag = true;
    }
}

static void speak(avatarlines::Event ev, const decklink::View& v, uint32_t now) {
    static bool spoke = false;
    static uint32_t lastAt = 0;
    static avatarlines::Out out;
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
            speak(avatarlines::Event::Boot, v, now);
        } else if (v.deck.trackGen != gen) {
            gen = v.deck.trackGen;
            speak(avatarlines::Event::Track, v, now);
        }
    }
    if (s_talking) {                             // her line: typing + hold (no slide: she is on)
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
