#pragma once
#include <cstddef>
#include <cstdint>
#include "songpalette.h"
// Music mode's colour for a song: one of the Lights grid's 8 swatches (ledlook's order: 0 red,
// 1 orange, 2 pink, 3 purple, 4 blue, 5 green, 6 white, 7 yellow), strongest signal first:
//  1. the owner's picks (songpicks.inc): a song (title + artist), an album or an artist -> a swatch;
//  2. a colour word (PT or EN, a whole word) in the title, then the album, then the playlist's name;
//  3. otherwise a mood gives a set of allowed swatches: the playlist's (its name, then its
//     description: words and emoji), else the genre badge's, else all 8 ("any");
//     the cover chooses inside it: the first cover colour (in weight order) whose nearest swatch is
//     allowed; else the allowed swatch nearest the first colour if within 45 deg of hue. Failing
//     that (a grey cover too): under a playlist's mood, the genre mood's first swatch that the
//     playlist's set allows; else the set's first. A white-ish colour counts only where white is allowed.
// Text matches ignore case and accents (fold); titles and albums also match without a suffix such
// as " - Remastered" or " (feat. ...)". The time of day changes only the brightness (lights()).
// PURE, host-tested.
namespace songmood {
// What the black deck knows of the song. Any field may be nullptr (= ""). playlist: the playlist's
// name, nullptr or "" when the song isn't playing from one (an album, an artist, Liked Songs);
// description: the playlist's, HTML already stripped.
struct Song {
    const char* title;
    const char* artist;
    const char* album;
    const char* playlist;
    const char* description;
};
enum class Rule : uint8_t { Pick, Word, PlaylistMood, GenreMood, Any };
struct Choice {
    int swatch;            // the grid swatch, 0..7
    Rule rule;             // what decided it
    const char* mood;      // the mood whose set applied ("neon", "intimate", ..., "any"), even for a pick
    uint8_t set[8];        // that set, strongest first
    uint8_t setN;
    const char* genre;     // the genre mood that chose inside a playlist's set (the cover gave none), or nullptr
};
Choice choose(const Song& song, const songpalette::Rgb* cover, int coverCount, uint8_t badge);
// The PalPkt palette for a choice: c[0] the swatch's screen colour (the yellow deck snaps it back to
// exactly that swatch), c[1] and c[2] the set's next swatches (informational), the hour's cap and depth.
songpalette::Palette lights(const Choice& c, int minuteOfDay);
const char* ruleName(Rule r);   // "pick", "word", "playlist mood", "genre mood", "any"

// UTF-8 -> lowercase ASCII: accented Latin letters lose their accent (Á -> a, ç -> c), anything
// else non-ASCII (emoji, CJK) is dropped. At most n bytes incl. NUL; returns the length.
size_t fold(const char* utf8, char* out, size_t n);
// word appears in folded text as a whole word (not inside a longer run of letters and digits).
bool hasWord(const char* folded, const char* word);
}  // namespace songmood
