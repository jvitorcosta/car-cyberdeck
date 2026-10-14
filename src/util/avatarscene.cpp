#include "avatarscene.h"
#include <cstring>

namespace avatarscene {
// Byte length of the first n characters of s; *total = its character count.
static size_t prefix(const char* s, size_t n, size_t* total) {
    size_t chars = 0, cut = 0, i = 0;
    for (; s[i]; ++i) {
        if (((unsigned char)s[i] & 0xC0) == 0x80) continue;   // UTF-8 continuation byte
        if (chars == n) cut = i;
        ++chars;
    }
    if (chars <= n) cut = i;
    *total = chars;
    return cut;
}

// Letters, digits and non-ASCII characters move the mouth; spaces and punctuation don't.
static bool voiced(unsigned char c) {
    return c >= 0x80 || (c >= '0' && c <= '9') || ((c | 0x20) >= 'a' && (c | 0x20) <= 'z');
}

State at(uint32_t ms, const char* line, int frames) {
    const char* s = line ? line : "";
    State st{Phase::Slide, SIDE, 0, 0};
    if (ms < SLIDE_MS) {
        st.shown = SIDE * (int)(ms * STEPS / SLIDE_MS + 1) / STEPS;
        return st;
    }
    const uint32_t t = ms - SLIDE_MS;
    size_t total = 0;
    const size_t typed = t / CHAR_MS;
    st.bytes = prefix(s, typed, &total);
    if (typed < total) {
        st.phase = Phase::Type;
        if (frames > 1 && st.bytes > 0 && voiced((unsigned char)s[st.bytes - 1]) &&
            (t / MOUTH_MS) % 2 == 0)
            st.frame = 1 + (int)((t / (2 * MOUTH_MS)) % (uint32_t)(frames - 1));
        return st;
    }
    st.bytes = strlen(s);
    st.phase = t < total * CHAR_MS + HOLD_MS ? Phase::Hold : Phase::Done;
    return st;
}
}
