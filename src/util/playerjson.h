#pragma once
#include <cstdint>
// Spotify's GET /v1/me/player answer (one request: the song, its progress and the device), read
// into what the deck shows. PURE, host-tested.
namespace playerjson {
enum class Kind : uint8_t { None, Track, Episode, Other };   // Other: an ad or an unknown item
struct Now {
    Kind kind;              // None: no item (nothing loaded on the device)
    bool playing;
    uint32_t progressMs, durationMs;
    char name[96];          // song or podcast episode
    char uri[160];          // spotify:track:... (local-file URIs can be long)
    char artist[96];        // first artist, or the podcast's show
    char album[96];         // "" for an episode
    char artUrl[160];       // the ~300 px image (second smallest), else the only one
    char context[96];       // spotify:playlist:... ("" when none)
    char deviceName[48];
    char deviceType[16];
    int volume;             // percent, -1 when the device doesn't say
    bool shuffle;
    int repeat;             // 0 = off, 1 = context, 2 = track
};
// False (out zeroed) unless body is a /me/player object (it has "is_playing"). Fields the answer
// lacks stay empty or 0; an Other item leaves the song fields empty. Strings are cut to fit.
bool parse(const char* body, Now& out);
}
