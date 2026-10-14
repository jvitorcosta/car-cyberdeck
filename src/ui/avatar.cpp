#include "avatar.h"
#include <Arduino.h>
#include <cstring>
#include "screen_now.h"
#include "../util/avatarbin.h"
#include "../util/avatarlines.h"
#include "../util/avatarscene.h"

#ifdef BOARD_S3   // only the S3 embeds data/avatar.bin (platformio.ini)
extern const uint8_t _binary_data_avatar_bin_start[] asm("_binary_data_avatar_bin_start");
extern const uint8_t _binary_data_avatar_bin_end[] asm("_binary_data_avatar_bin_end");
#endif

namespace ui {
static avatarbin::Info s_info{};
static const uint8_t* s_blob = nullptr;
static bool s_active = false;
static uint32_t s_t0 = 0;
static int s_entry = 0;
static char s_text[avatarlines::TEXT_CAP];
static int s_shown = -1, s_frame = -1;    // what is on screen now (-1: nothing yet)
static size_t s_bytes = (size_t)-1;       // typed prefix on screen ((size_t)-1: tag not drawn)

bool avatarBegin() {
    s_info.moods = 0;
#ifdef BOARD_S3
    const char* why = "";
    s_blob = _binary_data_avatar_bin_start;
    const size_t len = (size_t)(_binary_data_avatar_bin_end - _binary_data_avatar_bin_start);
    if (avatarbin::parse(s_blob, len, s_info, &why)) {
        Serial.printf("[avatar] %s: %d moods\n", s_info.name, s_info.moods);
        return true;
    }
    Serial.printf("[avatar] off: %s\n", why);
#endif
    return false;
}

bool avatarOn() { return s_info.moods > 0; }
bool avatarActive() { return s_active; }

void avatarStart(uint8_t mood, const char* text, uint32_t nowMs) {
    if (!avatarOn()) return;
    s_entry = avatarbin::find(s_info, mood);
    strlcpy(s_text, text ? text : "", sizeof s_text);
    s_t0 = nowMs;
    s_active = true;
    s_shown = s_frame = -1;
    s_bytes = (size_t)-1;
}

void avatarCancel() { s_active = false; }

// Frame f of entry e with its right `shown` columns at the left of the art box. Rows go through
// a RAM buffer: the embedded file need not be 2-byte aligned, and only visible columns are sent.
static void drawFrame(TFT_eSPI& t, int e, int f, int shown) {
    constexpr int N = avatarbin::SIDE;
    if (shown <= 0) return;
    if (shown > N) shown = N;
    const uint8_t* px = s_blob + avatarbin::frameOffset(s_info, e, f);
    const int skip = N - shown;               // columns still outside the box, on the left
    static uint16_t row[N];
    for (int y = 0; y < N; ++y) {
        memcpy(row, px + ((size_t)y * N + skip) * 2, (size_t)shown * 2);
        t.pushImage(ART_X, ART_Y + y, shown, 1, row);   // bytes as stored: RGB565 high byte first
    }
}

AvatarTick avatarTick(TFT_eSPI& t, uint32_t nowMs) {
    if (!s_active) return AvatarTick::Idle;
    const avatarscene::State st = avatarscene::at(nowMs - s_t0, s_text, s_info.frames[s_entry]);
    if (st.phase == avatarscene::Phase::Done) {
        s_active = false;
        return AvatarTick::Ended;
    }
    if (st.shown != s_shown || st.frame != s_frame) {
        drawFrame(t, s_entry, st.frame, st.shown);
        s_shown = st.shown;
        s_frame = st.frame;
    }
    if (st.phase != avatarscene::Phase::Slide && st.bytes != s_bytes) {
        if (s_bytes == (size_t)-1) drawNameTag(t, s_info.name);   // once, when typing starts
        char typed[avatarlines::TEXT_CAP];
        memcpy(typed, s_text, st.bytes);
        typed[st.bytes] = '\0';
        drawLyricArea(t, typed, false);
        s_bytes = st.bytes;
    }
    return AvatarTick::Running;
}
}
