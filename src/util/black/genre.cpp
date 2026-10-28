#include "genre.h"
#include <cstring>

namespace genre {

static bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

bool parsePrimaryGenreId(const char* body, uint32_t* id) {
    if (!body) return false;
    const char* p = strstr(body, "\"primaryGenreId\"");
    if (!p) return false;
    p += 16;                                   // strlen("\"primaryGenreId\"")
    while (isSpace(*p)) ++p;
    if (*p != ':') return false;
    ++p;
    while (isSpace(*p)) ++p;
    uint32_t v = 0;
    int digits = 0;
    while (*p >= '0' && *p <= '9') {
        if (++digits > 9) return false;        // no genre id is that long
        v = v * 10 + (uint32_t)(*p - '0');
        ++p;
    }
    if (digits == 0) return false;
    if (*p != ',' && *p != '}' && !isSpace(*p)) return false;   // cut off mid-number
    *id = v;
    return true;
}

Answer readAnswer(const char* body, uint32_t* id) {
    if (parsePrimaryGenreId(body, id)) return Answer::Found;
    if (!body) return Answer::Incomplete;
    const char* end = body + strlen(body);
    while (end > body && isSpace(end[-1])) --end;
    return (end > body && end[-1] == '}') ? Answer::NoMatch : Answer::Incomplete;
}

uint32_t hashName(const char* s) {
    uint32_t h = 2166136261u;
    for (; s && *s; ++s) { h ^= (uint8_t)*s; h *= 16777619u; }
    return h;
}

bool Cache::find(const char* artist, uint8_t* badge) {
    if (!artist || !artist[0]) { *badge = NONE; return true; }
    uint32_t h = hashName(artist);
    for (int i = 0; i < count_; ++i)
        if (hash_[i] == h) { *badge = badge_[i]; used_[i] = ++clock_; return true; }
    return false;
}

void Cache::store(const char* artist, uint8_t badge) {
    if (!artist || !artist[0]) return;
    uint32_t h = hashName(artist);
    for (int i = 0; i < count_; ++i)
        if (hash_[i] == h) { badge_[i] = badge; used_[i] = ++clock_; return; }
    int slot = 0;
    if (count_ < N) slot = count_++;
    else
        for (int i = 1; i < N; ++i)   // least recently used (a session never wraps the clock)
            if (used_[i] < used_[slot]) slot = i;
    hash_[slot] = h;
    badge_[slot] = badge;
    used_[slot] = ++clock_;
}

}
