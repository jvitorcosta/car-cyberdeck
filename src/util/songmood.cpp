#include "songmood.h"
#include <cstring>
#include <initializer_list>
#include "ledlook.h"
#include "musiclights.h"
#include "text.h"

namespace songmood {
namespace {

enum : uint8_t { RED, ORANGE, PINK, PURPLE, BLUE, GREEN, WHITE, YELLOW };   // ledlook's grid order

struct Pick {
    const char* artist;
    const char* title;   // a song; nullptr: the album, or every song of the artist
    const char* album;
    uint8_t swatch;
};
#include "songpicks.inc"

// A colour word in a title, an album or a playlist's name (folded: lowercase, no accents).
struct Word {
    const char* word;
    uint8_t swatch;
};
const Word WORDS[] = {
    {"purple", PURPLE}, {"purpura", PURPLE}, {"roxo", PURPLE},     {"violeta", PURPLE},
    {"red", RED},       {"vermelho", RED},   {"vermelha", RED},    {"blue", BLUE},
    {"azul", BLUE},     {"pink", PINK},      {"rosa", PINK},       {"green", GREEN},
    {"verde", GREEN},   {"yellow", YELLOW},  {"amarelo", YELLOW},  {"gold", YELLOW},
    {"dourado", YELLOW}, {"ouro", YELLOW},   {"orange", ORANGE},   {"laranja", ORANGE},
    {"white", WHITE},   {"branco", WHITE},
};

// A mood and the swatches it allows, strongest first. A playlist's mood comes from its words
// (folded, space-separated) or its emoji (UTF-8, space-separated, without the U+FE0F that some
// carry); the first mood in this order that its name matches, else its description.
struct Mood {
    const char* name;
    const char* words;
    const char* emoji;
    uint8_t set[8];
    uint8_t n;
};
const Mood PLAYLIST_MOODS[] = {
    {"neon", "night noite neon lights city midnight madrugada dark", "🌃 🌆 🌙 🌌", {RED, PURPLE, BLUE, PINK}, 4},
    {"intimate", "love amor cuddle cuddles moon lua romance romantic kiss slow sexy", "❤ 💕 💜 🌹 🌙",
     {RED, PINK, PURPLE}, 3},
    {"drive", "drive racing race speed hamilton f1 formula fast furious carro estrada road", "🏎 🏁 🚗",
     {RED, ORANGE, BLUE, YELLOW}, 4},
    {"energy", "workout treino gym academia run corrida hype party festa baile", "🔥 💪 🎉",
     {RED, ORANGE, PINK, YELLOW}, 4},
    {"calm", "chill relax calm calma sleep sono study estudo lofi lo-fi acoustic rain chuva", "🌧 ☕ 🌊",
     {BLUE, WHITE, PURPLE}, 3},
    {"sunny", "verao summer praia beach sol sun tropical", "☀ 🏖 🌴", {YELLOW, ORANGE, GREEN}, 3},
};

// The genre badge's mood (util/genre_data.inc's indices).
enum : uint8_t { M_INTIMATE, M_POP, M_IDOL, M_RAP, M_ROCK, M_ALT, M_ELECTRO, M_BRASIL, M_FUNK_BR, M_LATIN, M_SOFT };
const Mood GENRE_MOODS[] = {
    {"intimate", nullptr, nullptr, {RED, PINK, PURPLE}, 3},
    {"pop", nullptr, nullptr, {PINK, PURPLE, RED}, 3},
    {"idol", nullptr, nullptr, {PINK, PURPLE, BLUE}, 3},
    {"rap", nullptr, nullptr, {RED, PURPLE, PINK}, 3},
    {"rock", nullptr, nullptr, {RED, ORANGE}, 2},
    {"alt", nullptr, nullptr, {BLUE, PURPLE, RED}, 3},
    {"electro", nullptr, nullptr, {BLUE, PURPLE, PINK}, 3},
    {"brasil", nullptr, nullptr, {YELLOW, ORANGE, GREEN, PINK}, 4},
    {"funk br", nullptr, nullptr, {PINK, PURPLE, RED}, 3},
    {"latin", nullptr, nullptr, {ORANGE, YELLOW, RED}, 3},
    {"soft", nullptr, nullptr, {BLUE, WHITE, ORANGE}, 3},
};
const uint8_t BADGE_MOOD[] = {
    M_POP,        // 0 POP
    M_IDOL,       // 1 K-POP
    M_IDOL,       // 2 J-POP
    M_RAP,        // 3 RAP
    M_INTIMATE,   // 4 R&B
    M_RAP,        // 5 FUNK
    M_ROCK,       // 6 ROCK
    M_ROCK,       // 7 METAL
    M_ALT,        // 8 ALT
    M_ELECTRO,    // 9 ELECTRO
    M_ELECTRO,    // 10 DANCE
    M_BRASIL,     // 11 BRASIL
    M_FUNK_BR,    // 12 FUNK BR
    M_BRASIL,     // 13 SERTANEJO
    M_BRASIL,     // 14 MPB
    M_BRASIL,     // 15 SAMBA
    M_BRASIL,     // 16 PAGODE
    M_BRASIL,     // 17 FORRO
    M_BRASIL,     // 18 AXE
    M_LATIN,      // 19 LATIN
    M_BRASIL,     // 20 REGGAE
    M_SOFT,       // 21 JAZZ
    M_SOFT,       // 22 GOSPEL
    M_SOFT,       // 23 CLASSIC
    M_SOFT,       // 24 OST
    M_SOFT,       // 25 COUNTRY
    M_SOFT,       // 26 FOLK
    M_RAP,        // 27 AFRO
};                // past these (28 CHINESE, 29 WORLD, 30 ???, NONE): any
const Mood ANY = {"any", nullptr, nullptr, {RED, ORANGE, PINK, PURPLE, BLUE, GREEN, WHITE, YELLOW}, 8};

constexpr int NEAR_DEG = 45;   // a cover colour outside the set still picks the allowed swatch this near

constexpr size_t TEXT = 164;   // a folded title, album, playlist name or description (the longest: 160 B)
constexpr size_t NAME = 100;   // a folded artist or pick

bool isAlnum(char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'); }

// Where word first appears as a whole word in folded text, or -1.
int findWord(const char* text, const char* word) {
    const size_t n = strlen(word);
    if (!n) return -1;
    for (const char* p = strstr(text, word); p; p = strstr(p + 1, word)) {
        if ((p == text || !isAlnum(p[-1])) && !isAlnum(p[n])) return static_cast<int>(p - text);
    }
    return -1;
}

// Calls f(token, length) for each space-separated token of list until it returns true.
template <typename F>
bool anyToken(const char* list, F f) {
    for (const char* p = list; p && *p;) {
        while (*p == ' ') ++p;
        const char* e = p;
        while (*e && *e != ' ') ++e;
        if (e > p && f(p, static_cast<size_t>(e - p))) return true;
        p = e;
    }
    return false;
}

bool moodMatches(const Mood& m, const char* raw, const char* folded) {
    const bool word = anyToken(m.words, [folded](const char* w, size_t n) {
        char buf[24];
        if (n >= sizeof buf) return false;
        memcpy(buf, w, n);
        buf[n] = '\0';
        return findWord(folded, buf) >= 0;
    });
    return word || anyToken(m.emoji, [raw](const char* e, size_t n) {
        for (const char* p = raw; (p = strchr(p, e[0])) != nullptr; ++p)
            if (strncmp(p, e, n) == 0) return true;
        return false;
    });
}

// The text's length without a suffix such as " - Remastered", " (feat. ...)" or " [Live]".
size_t baseLength(const char* folded) {
    size_t n = strlen(folded);
    for (const char* cut : {" - ", " (", " ["}) {
        const char* p = strstr(folded, cut);
        if (p && static_cast<size_t>(p - folded) < n) n = static_cast<size_t>(p - folded);
    }
    return n;
}

// folded (a title or an album) names entry: all of it, or all of it but its suffix.
bool sameName(const char* folded, const char* entry) {
    char e[NAME];
    const size_t n = fold(entry, e, sizeof e);
    return n && (strcmp(folded, e) == 0 || (baseLength(folded) == n && strncmp(folded, e, n) == 0));
}

bool pick(const Song& s, int* swatch) {
    char artist[NAME], title[TEXT], album[TEXT], e[NAME];
    fold(s.artist, artist, sizeof artist);
    fold(s.title, title, sizeof title);
    fold(s.album, album, sizeof album);
    for (const Pick& p : PICKS) {
        fold(p.artist, e, sizeof e);
        if (!e[0] || strcmp(artist, e) != 0) continue;
        const bool match = p.title ? sameName(title, p.title) : p.album ? sameName(album, p.album) : true;
        if (match) {
            *swatch = p.swatch;
            return true;
        }
    }
    return false;
}

bool colourWord(const char* text, int* swatch) {
    char t[TEXT];
    fold(text, t, sizeof t);
    int best = -1;
    for (const Word& w : WORDS) {
        const int at = findWord(t, w.word);
        if (at >= 0 && (best < 0 || at < best)) {
            best = at;
            *swatch = w.swatch;
        }
    }
    return best >= 0;
}

const Mood* playlistMood(const Song& s) {
    if (!s.playlist || !s.playlist[0]) return nullptr;
    for (const char* raw : {s.playlist, s.description}) {
        if (!raw || !raw[0]) continue;
        char t[TEXT];
        fold(raw, t, sizeof t);
        for (const Mood& m : PLAYLIST_MOODS)
            if (moodMatches(m, raw, t)) return &m;
    }
    return nullptr;
}

// Hue in degrees and saturation in %, as musiclights measures them.
void hueSat(songpalette::Rgb c, int* hue, int* sat) {
    int mx = c.r, mn = c.r;
    if (c.g > mx) mx = c.g;
    if (c.b > mx) mx = c.b;
    if (c.g < mn) mn = c.g;
    if (c.b < mn) mn = c.b;
    const int d = mx - mn;
    *sat = mx ? d * 100 / mx : 0;
    if (!d) {
        *hue = 0;
        return;
    }
    int h;
    if (mx == c.r) h = 60 * (c.g - c.b) / d;
    else if (mx == c.g) h = 120 + 60 * (c.b - c.r) / d;
    else h = 240 + 60 * (c.r - c.g) / d;
    *hue = (h + 360) % 360;
}

songpalette::Rgb screen(int swatch) {
    const uint32_t s = ledlook::SWATCHES[swatch].screen;
    return {static_cast<uint8_t>(s >> 16), static_cast<uint8_t>((s >> 8) & 0xFF), static_cast<uint8_t>(s & 0xFF)};
}

bool allowed(const Mood& m, int swatch) {
    for (int i = 0; i < m.n; ++i)
        if (m.set[i] == swatch) return true;
    return false;
}

// The cover's swatch inside the set, or -1 when it gives none (no usable colour, or none allowed
// and none within NEAR_DEG).
int fromCover(const Mood& m, const songpalette::Rgb* cover, int count) {
    if (!cover || count <= 0) return -1;
    for (int i = 0; i < count; ++i) {
        const int sw = musiclights::nearestSwatch(cover[i]);   // a white-ish colour is white
        if (allowed(m, sw)) return sw;
    }
    int hue, sat;
    hueSat(cover[0], &hue, &sat);
    if (sat < musiclights::WHITE_SAT_PCT) return -1;   // no hue to be near
    int best = -1, bestD = NEAR_DEG + 1;
    for (int i = 0; i < m.n; ++i) {
        if (m.set[i] == WHITE) continue;
        int h, s;
        hueSat(screen(m.set[i]), &h, &s);
        int d = ((hue - h) % 360 + 360) % 360;
        if (d > 180) d = 360 - d;
        if (d < bestD) {
            bestD = d;
            best = m.set[i];
        }
    }
    return best;
}

}  // namespace

size_t fold(const char* utf8, char* out, size_t n) {
    const size_t len = txt::foldMarks(utf8, out, nullptr, n);
    for (size_t i = 0; i < len; ++i)
        if (out[i] >= 'A' && out[i] <= 'Z') out[i] = static_cast<char>(out[i] - 'A' + 'a');
    return len;
}

bool hasWord(const char* folded, const char* word) { return folded && word && findWord(folded, word) >= 0; }

Choice choose(const Song& song, const songpalette::Rgb* cover, int coverCount, uint8_t badge) {
    Choice c{};
    const Mood* m = playlistMood(song);
    c.rule = Rule::PlaylistMood;
    if (!m && badge < sizeof BADGE_MOOD) {
        m = &GENRE_MOODS[BADGE_MOOD[badge]];
        c.rule = Rule::GenreMood;
    }
    if (!m) {
        m = &ANY;
        c.rule = Rule::Any;
    }
    c.mood = m->name;
    memcpy(c.set, m->set, sizeof c.set);
    c.setN = m->n;
    if (pick(song, &c.swatch)) {
        c.rule = Rule::Pick;
    } else if (colourWord(song.title, &c.swatch) || colourWord(song.album, &c.swatch) ||
               (song.playlist && colourWord(song.playlist, &c.swatch))) {
        c.rule = Rule::Word;
    } else {
        c.swatch = fromCover(*m, cover, coverCount);
    }
    if (c.swatch < 0) {   // the cover gave none: the set's first, or under a playlist's mood the genre's
        c.swatch = m->set[0];
        const Mood* g = c.rule == Rule::PlaylistMood && badge < sizeof BADGE_MOOD ? &GENRE_MOODS[BADGE_MOOD[badge]]
                                                                                   : nullptr;
        for (int i = 0; g && i < g->n; ++i) {
            if (allowed(*m, g->set[i])) {   // the genre's first colour that the playlist's mood allows
                c.swatch = g->set[i];
                c.genre = g->name;
                break;
            }
        }
    }
    return c;
}

songpalette::Palette lights(const Choice& c, int minuteOfDay) {
    songpalette::Palette p{};
    p.c[0] = p.c[1] = p.c[2] = screen(c.swatch);
    int start = 0;   // after the swatch in its set, or the set's first
    for (int i = 0; i < c.setN; ++i)
        if (c.set[i] == c.swatch) start = i + 1;
    for (int k = 0, filled = 1; k < c.setN && filled < 3; ++k) {
        const int sw = c.set[(start + k) % c.setN];
        if (sw != c.swatch) p.c[filled++] = screen(sw);
    }
    songpalette::timeOfDay(minuteOfDay, &p.cap, &p.depth);
    return p;
}

const char* ruleName(Rule r) {
    switch (r) {
        case Rule::Pick: return "pick";
        case Rule::Word: return "word";
        case Rule::PlaylistMood: return "playlist mood";
        case Rule::GenreMood: return "genre mood";
        default: return "any";
    }
}

}  // namespace songmood
