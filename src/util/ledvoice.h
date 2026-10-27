#pragma once
// Spoken lights commands ("purple inside", "dimmer", "lights off", "azul 50%") -> what to change.
// The text is whatever Siri dictated, English or Portuguese, any case, with punctuation. Words it
// doesn't know are skipped, so "set the lights to purple please" works. PURE, host-tested.
#include <cctype>
#include <cstring>
#include "ledlook.h"

namespace ledvoice {
// Colours are ledlook's swatches (ledlook::Colour): the grid's eight by ledlook::COLOUR_WORDS, then
// light blue (a preset's colour, off the grid): "light blue", "azul claro", "cyan".
enum Zone { ZONE_NONE = -1, ZONE_BOTH = 0, ZONE_INSIDE = 1, ZONE_OUTSIDE = 2 };

struct Command {
    int color = -1;      // swatch index, or -1
    int power = -1;      // 1 on, 0 off, -1 unchanged
    int level = -1;      // absolute 10..100 %, or -1
    int levelStep = 0;   // +1 brighter, -1 dimmer (one 10 % step each)
    int zone = ZONE_NONE;
    int preset = -1;     // preset 1..6 ("rose", "favourite two"), or -1
};

namespace detail {
struct Word { const char* text; int value; };

inline int lookup(const char* w, const Word* table, int n) {
    for (int i = 0; i < n; i++) {
        if (std::strcmp(w, table[i].text) == 0) return table[i].value;
    }
    return -1;
}

using ledlook::BLUE, ledlook::LIGHT_BLUE, ledlook::PURPLE, ledlook::RED;
// Voice's colour words on top of ledlook::COLOUR_WORDS. Siri transcribes a lone colour word by sound:
// "red" comes back "Read", "blue" can come back "Blew" (seen in the deck's log, 2026-10-21). Neither
// homophone means anything else here, and neither belongs among a song title's words. Then "lilás"
// with its accent (the phrase isn't folded) and light blue's own.
static const Word COLORS[] = {
    {"read", RED}, {"blew", BLUE}, {"lil\xc3\xa1s", PURPLE}, {"cyan", LIGHT_BLUE}, {"ciano", LIGHT_BLUE},
};
// Light blue in two words: "light" right before a blue word, or "claro" / "clara" right after one
// ("azul claro"). Only next to each other, so "lights blue" and "light, then blue" stay blue, and
// either word alone is nothing.
static const char* const LIGHTER_BEFORE[] = {"light"};
static const char* const LIGHTER_AFTER[] = {"claro", "clara"};
// The swatch a colour word names, or -1.
inline int colour(const char* w) {
    for (const ledlook::ColourWord& c : ledlook::COLOUR_WORDS) {
        if (std::strcmp(w, c.word) == 0) return c.swatch;
    }
    return lookup(w, COLORS, static_cast<int>(sizeof(COLORS) / sizeof(COLORS[0])));
}
inline bool among(const char* w, const char* const* words, int n) {
    for (int i = 0; i < n; i++) {
        if (std::strcmp(w, words[i]) == 0) return true;
    }
    return false;
}
static const Word POWER[] = {
    {"off", 0}, {"desliga", 0}, {"desligar", 0}, {"desligue", 0}, {"apaga", 0}, {"apagar", 0}, {"apague", 0},
    {"on", 1},  {"liga", 1},    {"ligar", 1},    {"ligue", 1},    {"acende", 1}, {"acender", 1}, {"acenda", 1},
};
static constexpr int UP = 1, DOWN = 2;  // lookup() returns -1 for unknown words
static const Word STEPS[] = {
    {"brighter", UP}, {"up", UP},   {"more", UP},  {"increase", UP}, {"mais", UP}, {"aumenta", UP}, {"aumentar", UP}, {"forte", UP},
    {"dimmer", DOWN}, {"dim", DOWN}, {"down", DOWN}, {"less", DOWN}, {"lower", DOWN}, {"decrease", DOWN},
    {"menos", DOWN},  {"diminui", DOWN}, {"diminuir", DOWN}, {"fraco", DOWN}, {"fraca", DOWN},
};
static const Word LEVELS[] = {
    {"max", 100}, {"maximum", 100}, {"full", 100}, {"maximo", 100}, {"m\xc3\xa1ximo", 100},
    {"min", 10},  {"minimum", 10},  {"minimo", 10}, {"m\xc3\xadnimo", 10},
};
static const Word ZONES[] = {
    {"inside", ZONE_INSIDE},   {"interior", ZONE_INSIDE},  {"interno", ZONE_INSIDE}, {"dentro", ZONE_INSIDE},
    {"outside", ZONE_OUTSIDE}, {"exterior", ZONE_OUTSIDE}, {"externo", ZONE_OUTSIDE}, {"fora", ZONE_OUTSIDE},
    {"both", ZONE_BOTH},       {"all", ZONE_BOTH},         {"everything", ZONE_BOTH},
    {"todas", ZONE_BOTH},      {"todos", ZONE_BOTH},       {"ambos", ZONE_BOTH},     {"tudo", ZONE_BOTH},
};
// A favourite word arms the slot: the next number word or digit picks it. Siri hears "two" as "to"
// and "four" as "for", so those count, but only while armed ("set the lights to blue" stays a plain colour).
static const Word FAVS[] = {
    {"favourite", 1}, {"favorite", 1}, {"favourites", 1}, {"favorites", 1}, {"preset", 1},
};
static const Word SLOTS[] = {
    {"one", 1},   {"won", 1}, {"1", 1},
    {"two", 2},   {"to", 2},  {"too", 2}, {"2", 2},
    {"three", 3}, {"tree", 3}, {"3", 3},
    {"four", 4},  {"for", 4}, {"fore", 4}, {"4", 4},
    {"five", 5},  {"5", 5},
    {"six", 6},   {"6", 6},
};
// The six presets by name: ROSE, FIRE, GOJIRA (or "godzilla"), PURPURA ("purpura" is the owner's
// spelling), CIVIC, OCEAN. They set the preset directly, no arming. "rose" is not a colour word here
// (pink is "rosa"), so there is no clash.
static const Word PRESET_NAMES[] = {
    {"rose", 1}, {"fire", 2}, {"gojira", 3}, {"godzilla", 3}, {"purpura", 4}, {"civic", 5}, {"ocean", 6},
};
#define LEDVOICE_N(t) static_cast<int>(sizeof(t) / sizeof(t[0]))
}  // namespace detail

// Parses the phrase; false when it held nothing to do.
inline bool parse(const char* text, Command* out) {
    using namespace detail;
    *out = Command{};
    bool any = false;
    bool armed = false;  // a favourite word was heard and its slot number hasn't come yet
    bool light = false;  // the word before was "light"
    bool blue = false;   // the word before was a blue colour word
    char w[24];
    for (const char* p = text; *p;) {
        // a word: letters, digits and UTF-8 bytes (accents); everything else separates
        int n = 0;
        while (*p && (std::isalnum(static_cast<unsigned char>(*p)) || static_cast<unsigned char>(*p) >= 0x80)) {
            if (n < static_cast<int>(sizeof(w)) - 1) w[n++] = static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
            p++;
        }
        w[n] = 0;
        if (n == 0) {
            p++;
            continue;
        }
        const bool afterLight = light, afterBlue = blue;
        light = blue = false;
        if (afterBlue && among(w, LIGHTER_AFTER, LEDVOICE_N(LIGHTER_AFTER))) {  // "azul claro"
            out->color = LIGHT_BLUE;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(w[0]))) {  // "50", "50%": a level, or a slot right after "favourite"
            int v = 0;
            for (int i = 0; i < n && std::isdigit(static_cast<unsigned char>(w[i])); i++) v = v * 10 + (w[i] - '0');
            if (armed) {  // outside 1..6 there is no such favourite: preset stays -1 and nothing is parsed
                armed = false;
                if (v >= 1 && v <= 6) {
                    out->preset = v;
                    any = true;
                }
                continue;
            }
            v = (v + 5) / 10 * 10;
            out->level = v < 10 ? 10 : v > 100 ? 100 : v;
            any = true;
            continue;
        }
        int v;
        if (armed && (v = lookup(w, SLOTS, LEDVOICE_N(SLOTS))) >= 0) {
            out->preset = v;
            armed = false;
            any = true;
            continue;
        }
        if ((v = lookup(w, PRESET_NAMES, LEDVOICE_N(PRESET_NAMES))) >= 0) {
            out->preset = v;  // a command word: it also drops a pending "favourite" arm
            armed = false;
            any = true;
            continue;
        }
        if (lookup(w, FAVS, LEDVOICE_N(FAVS)) >= 0) {
            armed = true;  // not a command by itself: "favourite" alone parses to nothing
            continue;
        }
        if (among(w, LIGHTER_BEFORE, LEDVOICE_N(LIGHTER_BEFORE))) {
            light = true;  // not a command by itself ("light off" is just off); a blue word next makes it light blue
            continue;
        }
        if ((v = colour(w)) >= 0) {
            blue = v == BLUE;
            out->color = afterLight && blue ? static_cast<int>(LIGHT_BLUE) : v;
        }
        else if ((v = lookup(w, POWER, LEDVOICE_N(POWER))) >= 0) out->power = v;
        else if ((v = lookup(w, LEVELS, LEDVOICE_N(LEVELS))) >= 0) out->level = v;
        else if ((v = lookup(w, ZONES, LEDVOICE_N(ZONES))) >= 0) out->zone = v;
        else if ((v = lookup(w, STEPS, LEDVOICE_N(STEPS))) >= 0) out->levelStep = v == UP ? 1 : -1;
        else continue;  // not a command ("set", "the", "please", or "number" in "favourite number 2"): stays armed
        armed = false;  // a command word means no slot is coming
        any = true;
    }
    return any;
}
#undef LEDVOICE_N
}  // namespace ledvoice
