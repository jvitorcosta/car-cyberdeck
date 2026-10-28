#pragma once
// What one car light shows: a colour swatch, a brightness and on/off. The swatch table lives here so
// the panel firmware, the favourites and the voice commands all share one order. PURE, host-tested.
#include <cstdint>

namespace ledlook {
// Each colour has two values. `r,g,b` is what the strip gets: RGB LEDs don't mix like a screen (green
// and blue emitters look far brighter than red), so orange, pink and purple need much less green or
// blue than their screen colours, and white is warmed to offset the strips' blue cast. `screen` is
// what the tile shows: fully saturated, because the panel washes colours out (soft "LED-like" tints
// looked dead on it). `word` is the lowercase name used on the wire.
struct Swatch {
    const char* name;
    const char* word;
    uint8_t r, g, b;   // led
    uint32_t screen;   // 0xRRGGBB
};
// The first N_GRID are the Lights page's 2x4 grid, in ledvoice's colour order: red, orange, pink,
// purple, blue, green, white, yellow. Cyan, the last, is a preset's only (GOJIRA, Godzilla's atomic
// breath): never on the grid, never a song colour (musiclights snaps to the grid's eight); voice
// ("cyan", "light blue", "azul claro") and the lights' replies know it. One name everywhere a person
// reads it (the screen, voice replies, /state, /presets, the watch). Its LED mix, like purple's, is
// the one the owner picked by eye in the car on 2026-10-26.
// The swatches' indices, SWATCHES' order: every table of swatches (presets, colour words, song
// moods) names them by these.
enum Colour : uint8_t { RED, ORANGE, PINK, PURPLE, BLUE, GREEN, WHITE, YELLOW, CYAN };
constexpr int N_SWATCH = CYAN + 1, N_GRID = CYAN;
inline constexpr Swatch SWATCHES[N_SWATCH] = {
    {"Red", "red", 255, 0, 0, 0xFF0000},          {"Orange", "orange", 255, 40, 0, 0xFF6A00},
    {"Pink", "pink", 255, 0, 70, 0xFF0090},       {"Purple", "purple", 140, 0, 255, 0x8C00FF},
    {"Blue", "blue", 0, 0, 255, 0x0040FF},        {"Green", "green", 0, 255, 0, 0x00FF30},
    {"White", "white", 255, 215, 170, 0xFFFFFF},  {"Yellow", "yellow", 255, 110, 0, 0xFFD400},  // LED yellow: full green goes lime
    {"Cyan", "cyan", 0, 130, 255, 0x0082FF},
};

// The grid colour that stands for swatch sw where only the grid's eight can (the Lights page's
// focus, the terminal's theme): itself, or blue for cyan.
constexpr int gridSwatch(int sw) { return sw == CYAN ? static_cast<int>(BLUE) : sw; }

// The grid colours' words, English and Portuguese, lowercase and without accents: what voice commands
// (ledvoice) and song titles, albums, playlist names and #tags (songmood) call them. Cyan has
// none here (never a song colour): ledvoice adds its words, and the homophones Siri makes of a lone
// colour word, on top of these.
struct ColourWord {
    const char* word;
    uint8_t swatch;
};
inline constexpr ColourWord COLOUR_WORDS[] = {
    {"red", RED},         {"vermelho", RED},    {"vermelha", RED},
    {"orange", ORANGE},   {"laranja", ORANGE},
    {"pink", PINK},       {"rosa", PINK},       {"magenta", PINK},
    {"purple", PURPLE},   {"violet", PURPLE},   {"violeta", PURPLE}, {"purpura", PURPLE},
    {"roxo", PURPLE},     {"roxa", PURPLE},     {"lilac", PURPLE},   {"lilas", PURPLE},
    {"blue", BLUE},       {"azul", BLUE},
    {"green", GREEN},     {"verde", GREEN},
    {"white", WHITE},     {"branco", WHITE},    {"branca", WHITE},
    {"yellow", YELLOW},   {"amarelo", YELLOW},  {"amarela", YELLOW}, {"gold", YELLOW}, {"dourado", YELLOW}, {"ouro", YELLOW},
};
constexpr int N_COLOUR_WORDS = static_cast<int>(sizeof(COLOUR_WORDS) / sizeof(COLOUR_WORDS[0]));

constexpr uint8_t MIN_PCT = 10;

struct Look {
    int swatch = YELLOW;        // the grid's last
    uint8_t pct = 100;          // 10..100 %, in steps of 10
    bool lit = true;
};
inline bool operator==(const Look& a, const Look& b) {
    return a.swatch == b.swatch && a.pct == b.pct && a.lit == b.lit;
}
}  // namespace ledlook
