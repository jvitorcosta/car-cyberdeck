#pragma once
#include <cstddef>
#include <cstdint>
// Artist genre from Apple's iTunes Search API (spec 2026-10-08-genre-badge-design.md): read
// primaryGenreId out of the small JSON answer, and remember the badge per artist.
// PURE, host-tested.
namespace genre {
constexpr uint8_t NONE = 0xFF;   // Apple has no genre for this artist: no badge
// First "primaryGenreId": <digits> in body. False when absent, not a number, or cut off
// (digits must be followed by a delimiter: the read buffer may end inside the number).
bool parsePrimaryGenreId(const char* body, uint32_t* id);
uint32_t hashName(const char* s);   // FNV-1a 32-bit over the UTF-8 bytes
// How to treat an HTTP 200 body. Only a complete answer (ends with the closing '}') may say
// "no genre" (cached for the session); a body cut off by a dropped connection or the read
// buffer is Incomplete: a network error, not cached, looked up again next time.
enum class Answer { Found, NoMatch, Incomplete };
Answer readAnswer(const char* body, uint32_t* id);
// Badge index per artist, N artists, least recently used replaced (an artist on repeat stays).
// Callers store NONE for "Apple has no genre" and nothing for network errors, so those are
// looked up again next time.
class Cache {
public:
    static constexpr int N = 32;
    // True when the badge for `artist` is known (possibly NONE); a hit counts as a use. An
    // empty name is known: NONE.
    bool find(const char* artist, uint8_t* badge);
    void store(const char* artist, uint8_t badge);
private:
    uint32_t hash_[N] = {};
    uint32_t used_[N] = {};   // clock_ at the last find/store
    uint8_t badge_[N] = {};
    int count_ = 0;
    uint32_t clock_ = 0;
};
}
