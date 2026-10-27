#include <unity.h>
#include <cstring>
#include <initializer_list>
#include "../../src/util/black/genre.h"
#include "../../src/util/lights/ledlook.h"
#include "../../src/util/lights/musiclights.h"
#include "../../src/util/black/songmood.h"

using songmood::Choice;
using songmood::Rule;
using songmood::Song;
using songpalette::Rgb;

void setUp() {}
void tearDown() {}

namespace {
// ledlook's grid order
constexpr int RED = 0, ORANGE = 1, PINK = 2, PURPLE = 3, BLUE = 4, GREEN = 5, WHITE = 6, YELLOW = 7;
// genrebadge indices (util/genre_data.inc)
constexpr uint8_t POP = 0, RAP = 3, RNB = 4, ROCK = 6, ALT = 8, DANCE = 10, LATIN = 19, JAZZ = 21;

Rgb hex(uint32_t c) {
    return {static_cast<uint8_t>(c >> 16), static_cast<uint8_t>((c >> 8) & 0xFF), static_cast<uint8_t>(c & 0xFF)};
}

constexpr const char* NEON = "blinded by the lights \xF0\x9F\x8C\x83";      // 🌃
constexpr const char* HAMILTON = "through goes hamilton! \xF0\x9F\x8F\x8E\xEF\xB8\x8F";   // 🏎️

struct Case {
    const char* title;
    const char* artist;
    const char* album;
    const char* playlist;   // nullptr: not from a playlist
    uint8_t badge;
    uint32_t cover[3];
    int coverCount;
    int swatch;
    Rule rule;
};

Choice run(const Case& c) {
    Rgb cover[3];
    for (int i = 0; i < c.coverCount; ++i) cover[i] = hex(c.cover[i]);
    const Song s{c.title, c.artist, c.album, c.playlist, ""};
    return songmood::choose(s, cover, c.coverCount, c.badge);
}

void check(const Case* cases, int n) {
    for (int i = 0; i < n; ++i) {
        const Choice got = run(cases[i]);
        char msg[128];
        snprintf(msg, sizeof msg, "\"%s\": %s by %s", cases[i].title, ledlook::SWATCHES[got.swatch].word,
                 songmood::ruleName(got.rule));
        TEST_ASSERT_EQUAL_INT_MESSAGE(cases[i].swatch, got.swatch, msg);
        TEST_ASSERT_EQUAL_INT_MESSAGE(static_cast<int>(cases[i].rule), static_cast<int>(got.rule), msg);
    }
}
}  // namespace

// ---- folding and words ----

void test_fold_lowercases_and_drops_portuguese_accents() {
    char out[64];
    songmood::fold("T\xC3\x81 TARDE", out, sizeof out);   // TÁ TARDE
    TEST_ASSERT_EQUAL_STRING("ta tarde", out);
    songmood::fold("P\xC3\xBArpura", out, sizeof out);    // Púrpura
    TEST_ASSERT_EQUAL_STRING("purpura", out);
    // á é í ó ú â ê ô ã õ ç, then the capitals
    songmood::fold("\xC3\xA1\xC3\xA9\xC3\xAD\xC3\xB3\xC3\xBA\xC3\xA2\xC3\xAA\xC3\xB4\xC3\xA3\xC3\xB5\xC3\xA7 "
                   "\xC3\x81\xC3\x89\xC3\x8D\xC3\x93\xC3\x9A\xC3\x82\xC3\x8A\xC3\x94\xC3\x83\xC3\x95\xC3\x87",
                   out, sizeof out);
    TEST_ASSERT_EQUAL_STRING("aeiouaeoaoc aeiouaeoaoc", out);
    songmood::fold("Ver\xC3\xA3o \xE2\x98\x80\xEF\xB8\x8F na Praia", out, sizeof out);   // Verão ☀️ na Praia
    TEST_ASSERT_EQUAL_STRING("verao  na praia", out);                                    // the emoji goes
    char small[5];
    TEST_ASSERT_EQUAL_size_t(4, songmood::fold("ABCDEFG", small, sizeof small));
    TEST_ASSERT_EQUAL_STRING("abcd", small);
    songmood::fold(nullptr, out, sizeof out);
    TEST_ASSERT_EQUAL_STRING("", out);
}

void test_whole_words_only() {
    TEST_ASSERT_TRUE(songmood::hasWord("blinded by the lights", "lights"));
    TEST_ASSERT_FALSE(songmood::hasWord("blinding lights", "light"));
    TEST_ASSERT_FALSE(songmood::hasWord("goldfrapp", "gold"));
    TEST_ASSERT_TRUE(songmood::hasWord("gold", "gold"));
    TEST_ASSERT_TRUE(songmood::hasWord("chill lo-fi beats", "lo-fi"));
    TEST_ASSERT_TRUE(songmood::hasWord("f1 2024!", "f1"));
    TEST_ASSERT_FALSE(songmood::hasWord("f12", "f1"));
    TEST_ASSERT_FALSE(songmood::hasWord("bluetooth blues", "blue"));
    TEST_ASSERT_FALSE(songmood::hasWord("", "blue"));
}

// ---- the owner's logged songs (2026-10-27) ----

void test_logged_songs_in_the_neon_playlist() {
    const Case cases[] = {
        {"Popular", "The Weeknd", "Popular", NEON, RNB, {0xFF0800, 0xFF9000}, 2, RED, Rule::PlaylistMood},
        {"Levitating", "Dua Lipa", "Future Nostalgia", NEON, POP, {0x00AAFF, 0xFF1D00, 0xFF0066}, 3, BLUE,
         Rule::PlaylistMood},
        {"Open Hearts", "The Weeknd", "Hurry Up Tomorrow", NEON, RNB, {0xFF6100}, 1, RED, Rule::PlaylistMood},
        {"Blinding Lights", "The Weeknd", "After Hours", NEON, RNB, {0xFF0000}, 1, RED, Rule::Pick},
        {"Out of Time", "The Weeknd", "Dawn FM", NEON, RNB, {0x00D9FF}, 1, BLUE, Rule::Pick},
        {"Don't Break My Heart", "The Weeknd", "Dawn FM", NEON, RNB, {0x00D9FF}, 1, BLUE, Rule::PlaylistMood},
        {"T\xC3\x81 TARDE", "Vulgo FK", "F**K SONGS", NEON, RAP, {0xFF0077}, 1, PINK, Rule::Pick},
    };
    check(cases, sizeof cases / sizeof cases[0]);
}

void test_logged_songs_in_the_drive_playlist() {
    const Case cases[] = {
        {"Danza Kuduro", "Don Omar", "Meet The Orphans", HAMILTON, LATIN, {0xFF9000}, 1, ORANGE, Rule::PlaylistMood},
        // grey: pop's first that drive allows (SC2's pop set: pink, purple, blue)
        {"I'm Still Standing", "Elton John", "Too Low For Zero", HAMILTON, POP, {}, 0, BLUE, Rule::PlaylistMood},
        {"Lapdance", "N.E.R.D", "In Search Of...", HAMILTON, RAP, {0xFF5D00}, 1, ORANGE, Rule::PlaylistMood},
        {"Act A Fool", "Ludacris", "2 Fast 2 Furious", HAMILTON, RAP, {0x00AAFF, 0xFF1100, 0xFF9900}, 3, BLUE,
         Rule::PlaylistMood},
        {"DEJA VU", "dave rodgers", "Super Eurobeat", HAMILTON, DANCE, {0xFFD400, 0xFF1900}, 2, YELLOW,
         Rule::PlaylistMood},
        {"I Want to Break Free", "Queen", "The Works", HAMILTON, ROCK, {0xFF8800}, 1, ORANGE, Rule::PlaylistMood},
        {"Radioactive", "Imagine Dragons", "Night Visions", HAMILTON, ALT, {0x1100FF, 0x0077FF}, 2, BLUE,
         Rule::PlaylistMood},
        {"Smooth Operator - Single Version", "Sade", "Diamond Life", HAMILTON, RNB, {0x0000FF}, 1, BLUE,
         Rule::PlaylistMood},
    };
    check(cases, sizeof cases / sizeof cases[0]);
}

void test_logged_extras() {
    const Case cases[] = {
        {"P\xC3\xBArpura", "Anyone", "Some Album", nullptr, POP, {0xFF0000}, 1, PURPLE, Rule::Word},
        {"Azul", "Djavan", "Luz", nullptr, 14, {0xFFD400}, 1, BLUE, Rule::Word},
        {"Pausa", "Vulgo FK", "F**K SONGS", nullptr, RAP, {0x00FF30}, 1, PINK, Rule::Pick},
        // no playlist (album context): the genre's mood; R&B's intimate has no orange, red is near
        {"Open Hearts", "The Weeknd", "Hurry Up Tomorrow", nullptr, RNB, {0xFF6100}, 1, RED, Rule::GenreMood},
        // an unknown genre and no playlist: all 8, the cover decides
        {"Somewhere", "Nobody", "Nowhere", nullptr, genre::NONE, {0x00FF30}, 1, GREEN, Rule::Any},
        {"Somewhere", "Nobody", "Nowhere", nullptr, genre::NONE, {}, 0, RED, Rule::Any},
    };
    check(cases, sizeof cases / sizeof cases[0]);
}

// ---- the rules, one by one ----

void test_picks_ignore_case_accents_and_suffixes_and_need_the_artist() {
    const Case cases[] = {
        {"Blinding Lights - Remastered", "The Weeknd", "X", nullptr, RNB, {0x0040FF}, 1, RED, Rule::Pick},
        {"Save Your Tears (with Ariana Grande)", "The Weeknd", "X", nullptr, RNB, {0x0040FF}, 1, RED, Rule::Pick},
        {"OUT OF TIME (feat. Someone)", "the weeknd", "X", nullptr, RNB, {0xFF0000}, 1, BLUE, Rule::Pick},
        {"I Was Never There", "THE WEEKND", "X", nullptr, RNB, {0x0040FF}, 1, RED, Rule::Pick},
        {"Out of Time - Single Version", "The Weeknd", "X", nullptr, RNB, {0xFF0000}, 1, BLUE, Rule::Pick},
        // another artist's song of the same name is no pick
        {"Blinding Lights", "A Cover Band", "X", nullptr, RNB, {0x8C00FF}, 1, PURPLE, Rule::GenreMood},
        // the album pick, whatever the case
        {"Anything", "Vulgo FK", "f**k songs", nullptr, RAP, {0x0040FF}, 1, PINK, Rule::Pick},
        {"Anything", "Vulgo FK", "Another Album", nullptr, RAP, {0x8C00FF}, 1, PURPLE, Rule::GenreMood},
    };
    check(cases, sizeof cases / sizeof cases[0]);
}

void test_a_pick_beats_a_colour_word_which_beats_the_mood() {
    const Case cases[] = {
        {"Blinding Lights", "The Weeknd", "X", "blue night", RNB, {0x0040FF}, 1, RED, Rule::Pick},
        {"Red", "Anyone", "X", NEON, RNB, {0x0040FF}, 1, RED, Rule::Word},
        // title, then album, then the playlist's name
        {"Red", "Anyone", "Blue", "green", POP, {}, 0, RED, Rule::Word},
        {"Nothing", "Anyone", "Kind of Blue", "green", POP, {}, 0, BLUE, Rule::Word},
        {"Nothing", "Anyone", "Nothing", "Verde e amarelo", POP, {}, 0, GREEN, Rule::Word},
        // every colour word, PT and EN
        {"Roxo", "A", "X", nullptr, POP, {}, 0, PURPLE, Rule::Word},
        {"Violeta", "A", "X", nullptr, POP, {}, 0, PURPLE, Rule::Word},
        {"Vermelha", "A", "X", nullptr, POP, {}, 0, RED, Rule::Word},
        {"Rosa", "A", "X", nullptr, POP, {}, 0, PINK, Rule::Word},
        {"Pink + White", "A", "X", nullptr, POP, {}, 0, PINK, Rule::Word},
        {"Dourado", "A", "X", nullptr, POP, {}, 0, YELLOW, Rule::Word},
        {"Ouro", "A", "X", nullptr, POP, {}, 0, YELLOW, Rule::Word},
        {"Laranja", "A", "X", nullptr, POP, {}, 0, ORANGE, Rule::Word},
        {"Branco", "A", "X", nullptr, POP, {}, 0, WHITE, Rule::Word},
        // not whole words, or the artist's name: no colour word
        {"Utopia", "Goldfrapp", "Felt Mountain", nullptr, POP, {0xFF0090}, 1, PINK, Rule::GenreMood},
        {"Californication", "Red Hot Chili Peppers", "Californication", nullptr, ROCK, {0xFF6A00}, 1, ORANGE,
         Rule::GenreMood},
        {"Blinding Lights", "Somebody", "Bluetooth", nullptr, ROCK, {0xFF6A00}, 1, ORANGE, Rule::GenreMood},
    };
    check(cases, sizeof cases / sizeof cases[0]);
}

void test_playlist_moods_by_word_and_emoji() {
    const Case cases[] = {
        {"Song", "A", "X", "Noite na cidade", POP, {0x0040FF}, 1, BLUE, Rule::PlaylistMood},          // neon
        {"Song", "A", "X", "\xF0\x9F\x8C\x86", POP, {0x0040FF}, 1, BLUE, Rule::PlaylistMood},         // 🌆 neon
        {"Song", "A", "X", "amor \xE2\x9D\xA4\xEF\xB8\x8F", POP, {0x8C00FF}, 1, PURPLE, Rule::PlaylistMood},   // intimate
        {"Song", "A", "X", "estrada", POP, {0x00FF30}, 1, BLUE, Rule::PlaylistMood},   // drive: green is far: pop's blue
        {"Song", "A", "X", "TREINO \xF0\x9F\x94\xA5", POP, {0xEA00FF}, 1, PINK, Rule::PlaylistMood},   // energy: violet, pink near
        {"Song", "A", "X", "lo-fi to study", POP, {0xFF0000, 0x8C00FF}, 2, PURPLE, Rule::PlaylistMood},   // calm
        {"Song", "A", "X", "Ver\xC3\xA3o", POP, {0x00FF30}, 1, GREEN, Rule::PlaylistMood},             // sunny
        {"Song", "A", "X", "\xF0\x9F\x8C\xB4", POP, {}, 0, YELLOW, Rule::PlaylistMood},                 // 🌴 sunny
        // a playlist with no mood: the genre's
        {"Song", "A", "X", "my mix", ROCK, {0x0040FF}, 1, RED, Rule::GenreMood},
    };
    check(cases, sizeof cases / sizeof cases[0]);
}

void test_the_description_counts_after_the_name() {
    Rgb cover[1] = {hex(0xEA00FF)};   // a violet: purple's swatch, 31 deg from pink's
    Song s{"Song", "A", "X", "my mix", "songs for the gym &amp; more"};
    Choice c = songmood::choose(s, cover, 1, ROCK);
    TEST_ASSERT_EQUAL(Rule::PlaylistMood, c.rule);
    TEST_ASSERT_EQUAL_STRING("energy", c.mood);
    TEST_ASSERT_EQUAL_INT(PINK, c.swatch);   // purple isn't energy's: pink is near
    s.playlist = "chill";                     // the name's mood first
    c = songmood::choose(s, cover, 1, ROCK);
    TEST_ASSERT_EQUAL_STRING("calm", c.mood);
    TEST_ASSERT_EQUAL_INT(PURPLE, c.swatch);
    s.playlist = nullptr;                     // no playlist: its description doesn't count either
    c = songmood::choose(s, cover, 1, ROCK);
    TEST_ASSERT_EQUAL(Rule::GenreMood, c.rule);
}

void test_genre_moods() {
    struct G { uint8_t badge; int first; const char* mood; };
    const G gs[] = {
        {RNB, PURPLE, "intimate"}, {POP, PINK, "pop"}, {1, PINK, "idol"}, {2, PINK, "idol"}, {RAP, RED, "rap"},
        {5, RED, "funk"}, {27, RED, "funk"}, {ROCK, RED, "rock"}, {7, RED, "rock"}, {ALT, BLUE, "alt"},
        {9, BLUE, "electro"}, {DANCE, BLUE, "electro"}, {11, YELLOW, "brasil"}, {12, PINK, "funk br"},
        {13, YELLOW, "brasil"}, {14, YELLOW, "brasil"}, {18, YELLOW, "brasil"}, {LATIN, ORANGE, "latin"},
        {20, GREEN, "reggae"}, {JAZZ, BLUE, "soft"}, {22, BLUE, "soft"}, {23, BLUE, "soft"}, {24, BLUE, "ost"},
        {25, BLUE, "soft"}, {26, BLUE, "soft"}, {28, RED, "any"}, {30, RED, "any"}, {genre::NONE, RED, "any"},
    };
    for (const G& g : gs) {
        const Song s{"Song", "A", "X", nullptr, nullptr};
        const Choice c = songmood::choose(s, nullptr, 0, g.badge);   // black-and-white cover: the set's first
        TEST_ASSERT_EQUAL_INT_MESSAGE(g.first, c.swatch, g.mood);
        TEST_ASSERT_EQUAL_STRING(g.mood, c.mood);
        TEST_ASSERT_EQUAL(g.badge <= 27 ? Rule::GenreMood : Rule::Any, c.rule);
    }
}

void test_the_cover_chooses_inside_the_set() {
    const Song s{"Song", "A", "X", NEON, ""};   // red, purple, blue, pink
    // the first cover colour whose swatch is allowed, in weight order
    Rgb c1[3] = {hex(0x00FF30), hex(0xFFD400), hex(0x8C00FF)};
    TEST_ASSERT_EQUAL_INT(PURPLE, songmood::choose(s, c1, 3, POP).swatch);
    // none allowed: the allowed swatch nearest the first colour within 45 deg, else the set's first
    Rgb c2[1] = {hex(0xFF6A00)};   // orange, 25 deg: red
    TEST_ASSERT_EQUAL_INT(RED, songmood::choose(s, c2, 1, POP).swatch);
    Rgb c3[1] = {hex(0x00FF30)};   // green: 94 deg from blue
    TEST_ASSERT_EQUAL_INT(RED, songmood::choose(s, c3, 1, genre::NONE).swatch);
    TEST_ASSERT_EQUAL_INT(PINK, songmood::choose(s, c3, 1, POP).swatch);   // pop's first that neon allows
    // a white-ish colour counts as white only where white is allowed
    Rgb c4[2] = {hex(0xE0E0E0), hex(0x0040FF)};
    TEST_ASSERT_EQUAL_INT(BLUE, songmood::choose(s, c4, 2, POP).swatch);
    const Song calm{"Song", "A", "X", "chill", ""};   // blue, white, purple
    TEST_ASSERT_EQUAL_INT(WHITE, songmood::choose(calm, c4, 2, POP).swatch);
    Rgb c5[1] = {hex(0xE0E0E0)};
    TEST_ASSERT_EQUAL_INT(RED, songmood::choose(s, c5, 1, genre::NONE).swatch);   // no hue to be near: the first
    TEST_ASSERT_EQUAL_INT(PINK, songmood::choose(s, c5, 1, POP).swatch);
    // a null cover with a count is no cover
    TEST_ASSERT_EQUAL_INT(RED, songmood::choose(s, nullptr, 2, genre::NONE).swatch);
}

// Replay 2026-10-27 in the neon playlist: a grey cover (or none near the set) takes the genre
// mood's first swatch that the playlist's set allows; songs the cover decides stay as they were.
void test_replay_grey_covers_take_the_genres_colour_in_the_playlists_mood() {
    const Case cases[] = {
        {"Love Me Harder", "Ariana Grande", "My Everything", NEON, POP, {}, 0, PINK, Rule::PlaylistMood},
        {"\xE3\x83\x97\xE3\x83\xA9\xE3\x82\xB9\xE3\x83\x86\xE3\x82\xA3\xE3\x83\x83\xE3\x82\xAF\xE3\x83\xBB"
         "\xE3\x83\xA9\xE3\x83\x96",   // プラスティック・ラブ
         "Mariya Takeuchi", "Variety", NEON, 2, {}, 0, PINK, Rule::PlaylistMood},
        {"SexyBack (feat. Timbaland)", "Justin Timberlake", "FutureSex/LoveSounds", NEON, POP, {}, 0, PINK,
         Rule::PlaylistMood},
        // unchanged
        {"Wake Me Up (feat. Justice)", "The Weeknd", "Hurry Up Tomorrow", NEON, RNB, {0xFF6100}, 1, RED,
         Rule::PlaylistMood},
        {"Superman", "Eminem", "The Eminem Show", NEON, RAP, {0xFF1100}, 1, RED, Rule::PlaylistMood},
        {"Kiss Me More (feat. SZA)", "Doja Cat", "Planet Her", NEON, RNB, {0x0066FF, 0x5500FF}, 2, BLUE,
         Rule::PlaylistMood},
        {"Sunflower - Spider-Man: Into the Spider-Verse", "Post Malone", "Spider-Man", NEON, RAP, {0x00E6FF}, 1, BLUE,
         Rule::PlaylistMood},
        {"No Role Modelz", "J. Cole", "2014 Forest Hills Drive", NEON, RAP, {0x00A2FF, 0x73FF00}, 2, BLUE,
         Rule::PlaylistMood},
        {"PAUSA", "Vulgo FK", "F**K SONGS", NEON, RAP, {}, 0, PINK, Rule::Pick},
        {"Midnight Pretenders", "Tomoko Aran", "Fuyu-Kukan", NEON, 2, {0x0077FF, 0xFF006B}, 2, BLUE,
         Rule::PlaylistMood},
    };
    check(cases, sizeof cases / sizeof cases[0]);
}

void test_the_genre_fallback_and_what_it_leaves_alone() {
    const Song neon{"Song", "A", "X", NEON, ""};
    Choice c = songmood::choose(neon, nullptr, 0, POP);
    TEST_ASSERT_EQUAL_INT(PINK, c.swatch);
    TEST_ASSERT_EQUAL_STRING("neon", c.mood);
    TEST_ASSERT_EQUAL_STRING("pop", c.genre);   // the log's "playlist mood neon, genre pop"
    // the cover decided: no genre named
    Rgb blue[1] = {hex(0x0040FF)};
    TEST_ASSERT_NULL(songmood::choose(neon, blue, 1, POP).genre);
    // no overlap (sunny has none of pop's), or no genre: the playlist set's first
    const Song sunny{"Song", "A", "X", "summer", ""};
    c = songmood::choose(sunny, nullptr, 0, POP);
    TEST_ASSERT_EQUAL_INT(YELLOW, c.swatch);
    TEST_ASSERT_NULL(c.genre);
    TEST_ASSERT_EQUAL_INT(RED, songmood::choose(neon, nullptr, 0, 29).swatch);   // WORLD: any, not a mood
    // the set came from the genre itself: its first, as before
    const Song album{"Song", "A", "X", nullptr, nullptr};
    c = songmood::choose(album, nullptr, 0, ROCK);
    TEST_ASSERT_EQUAL_INT(RED, c.swatch);
    TEST_ASSERT_NULL(c.genre);
}

void test_null_fields_are_empty() {
    const Song s{nullptr, nullptr, nullptr, nullptr, nullptr};
    const Choice c = songmood::choose(s, nullptr, 0, genre::NONE);
    TEST_ASSERT_EQUAL_INT(RED, c.swatch);
    TEST_ASSERT_EQUAL(Rule::Any, c.rule);
}

// ---- what goes on the wire ----

void test_lights_send_the_swatchs_screen_colour_and_the_hours_cap() {
    const Song s{"Levitating", "Dua Lipa", "X", NEON, ""};
    Rgb cover[1] = {hex(0x00AAFF)};
    const Choice c = songmood::choose(s, cover, 1, POP);
    TEST_ASSERT_EQUAL_INT(BLUE, c.swatch);
    for (int minute : {-1, 720, 1200, 1380}) {
        const songpalette::Palette p = songmood::lights(c, minute);
        TEST_ASSERT_EQUAL_INT(BLUE, musiclights::nearestSwatch(p.c[0]));   // the yellow deck's snap
        TEST_ASSERT_EQUAL_HEX8(0x00, p.c[0].r);
        TEST_ASSERT_EQUAL_HEX8(0x40, p.c[0].g);
        TEST_ASSERT_EQUAL_HEX8(0xFF, p.c[0].b);
        // the set's next swatches after blue: pink, then red
        TEST_ASSERT_EQUAL_INT(PINK, musiclights::nearestSwatch(p.c[1]));
        TEST_ASSERT_EQUAL_INT(RED, musiclights::nearestSwatch(p.c[2]));
        uint8_t cap, depth;
        songpalette::timeOfDay(minute, &cap, &depth);
        TEST_ASSERT_EQUAL_UINT8(cap, p.cap);
        TEST_ASSERT_EQUAL_UINT8(depth, p.depth);
    }
    // a pick outside its mood's set: the set's first two after it
    const Song pick{"Out of Time", "The Weeknd", "Dawn FM", "summer", ""};   // sunny: yellow, orange, green
    const Choice pc = songmood::choose(pick, nullptr, 0, RNB);
    TEST_ASSERT_EQUAL_INT(BLUE, pc.swatch);
    const songpalette::Palette pp = songmood::lights(pc, 720);
    TEST_ASSERT_EQUAL_INT(YELLOW, musiclights::nearestSwatch(pp.c[1]));
    TEST_ASSERT_EQUAL_INT(ORANGE, musiclights::nearestSwatch(pp.c[2]));
}

void test_rule_names() {
    TEST_ASSERT_EQUAL_STRING("pick", songmood::ruleName(Rule::Pick));
    TEST_ASSERT_EQUAL_STRING("word", songmood::ruleName(Rule::Word));
    TEST_ASSERT_EQUAL_STRING("tags", songmood::ruleName(Rule::Tags));
    TEST_ASSERT_EQUAL_STRING("playlist mood", songmood::ruleName(Rule::PlaylistMood));
    TEST_ASSERT_EQUAL_STRING("genre mood", songmood::ruleName(Rule::GenreMood));
    TEST_ASSERT_EQUAL_STRING("any", songmood::ruleName(Rule::Any));
}

// ---- Task SC2: playlist colour tags, the agreed genre table, 60 deg, Budah ----

namespace {
Choice inPlaylist(const char* title, const char* name, const char* description, uint8_t badge, const Rgb* cover,
                  int count) {
    const Song s{title, "Someone", "Some Album", name, description};
    return songmood::choose(s, cover, count, badge);
}
}  // namespace

void test_tags_in_the_description() {
    Rgb blue[1] = {hex(0x0040FF)}, red[1] = {hex(0xFF0000)}, green[1] = {hex(0x00FF30)};
    // one tag: every song in it, any cover
    for (const Rgb* c : {blue, red, green}) {
        const Choice w = inPlaylist("Song", "my mix", "late drives #white", POP, c, 1);
        TEST_ASSERT_EQUAL_INT(WHITE, w.swatch);
        TEST_ASSERT_EQUAL(Rule::Tags, w.rule);
        TEST_ASSERT_EQUAL_STRING("tags", w.mood);
    }
    TEST_ASSERT_EQUAL_INT(WHITE, inPlaylist("Song", "my mix", "#white", RNB, nullptr, 0).swatch);
    // several: the allowed set, in the owner's order; the cover chooses inside it
    Choice c = inPlaylist("Song", "my mix", "#red #pink #purple", genre::NONE, blue, 1);
    TEST_ASSERT_EQUAL_INT(PURPLE, c.swatch);   // blue isn't in it; purple is 48 deg off
    TEST_ASSERT_EQUAL_INT(3, c.setN);
    TEST_ASSERT_EQUAL_INT(RED, c.set[0]);
    TEST_ASSERT_EQUAL_INT(PINK, c.set[1]);
    TEST_ASSERT_EQUAL_INT(PURPLE, c.set[2]);
    // a grey cover: the first tag, or (as for a playlist's mood) the genre's first inside the set
    c = inPlaylist("Song", "my mix", "#red #pink #purple", genre::NONE, nullptr, 0);
    TEST_ASSERT_EQUAL_INT(RED, c.swatch);
    TEST_ASSERT_NULL(c.genre);
    c = inPlaylist("Song", "my mix", "#red #pink #purple", RNB, nullptr, 0);
    TEST_ASSERT_EQUAL_INT(PURPLE, c.swatch);   // R&B: purple, red, pink
    TEST_ASSERT_EQUAL_STRING("intimate", c.genre);   // the log's "tags, genre intimate"
    // PT words, accents, case, duplicates once
    TEST_ASSERT_EQUAL_INT(BLUE, inPlaylist("Song", "x", "#azul", POP, red, 1).swatch);
    c = inPlaylist("Song", "x", "#P\xC3\xBARPURA #ROXA #Verde, #purple", genre::NONE, nullptr, 0);   // #PÚRPURA
    TEST_ASSERT_EQUAL_INT(2, c.setN);
    TEST_ASSERT_EQUAL_INT(PURPLE, c.set[0]);
    TEST_ASSERT_EQUAL_INT(GREEN, c.set[1]);
    // not tags: a longer word, an unknown one, a colour word without '#'
    TEST_ASSERT_EQUAL(Rule::GenreMood, inPlaylist("Song", "x", "#redemption", ROCK, red, 1).rule);
    TEST_ASSERT_EQUAL(Rule::GenreMood, inPlaylist("Song", "x", "#vibes #2024 red", ROCK, red, 1).rule);
    // only for a playlist (an album context has no description to read)
    const Song album{"Song", "A", "X", nullptr, "#white"};
    TEST_ASSERT_EQUAL(Rule::GenreMood, songmood::choose(album, red, 1, ROCK).rule);
}

void test_tags_beat_the_playlist_name_but_not_a_pick_or_the_title() {
    Rgb red[1] = {hex(0xFF0000)};
    TEST_ASSERT_EQUAL_INT(WHITE, inPlaylist("Song", "Blue Nights", "#white", POP, red, 1).swatch);   // name's word
    const Song albumWord{"Song", "Someone", "Kind of Blue", "x", "#white"};
    TEST_ASSERT_EQUAL_INT(WHITE, songmood::choose(albumWord, red, 1, POP).swatch);
    const Choice t = inPlaylist("P\xC3\xBArpura", "x", "#white", POP, red, 1);   // Púrpura
    TEST_ASSERT_EQUAL_INT(PURPLE, t.swatch);
    TEST_ASSERT_EQUAL(Rule::Word, t.rule);
    const Song pick{"Blinding Lights", "The Weeknd", "After Hours", "x", "#white"};
    TEST_ASSERT_EQUAL(Rule::Pick, songmood::choose(pick, red, 1, RNB).rule);
    TEST_ASSERT_EQUAL_INT(RED, songmood::choose(pick, red, 1, RNB).swatch);
}

void test_picks_song_then_album_then_artist() {
    const Case cases[] = {
        {"Anything", "Budah", "Any Album", nullptr, 14, {0xFFD400}, 1, PURPLE, Rule::Pick},
        {"Outra", "budah", "X", "#white", 14, {}, 0, PURPLE, Rule::Pick},
    };
    check(cases, sizeof cases / sizeof cases[0]);
}

void test_near_hue_is_60_degrees() {
    // a blue cover in an intimate set (red, pink, purple): purple, 48 deg off
    const Case cases[] = {
        {"Song", "A", "X", "love songs", POP, {0x0040FF}, 1, PURPLE, Rule::PlaylistMood},
        // an 81 deg lime snaps to yellow, not rock's; orange is 56 deg off (at 45 it fell back to red)
        {"Song", "A", "X", nullptr, ROCK, {0xA6FF00}, 1, ORANGE, Rule::GenreMood},
    };
    check(cases, sizeof cases / sizeof cases[0]);
}

void test_logged_songs_sc2() {
    const char* cuddles = "Cuddles 'n' Moans";
    const Case cases[] = {
        {"Best Part (feat. H.E.R.)", "Daniel Caesar", "Freudian", cuddles, RNB, {0x00B7FF}, 1, PURPLE,
         Rule::PlaylistMood},
        {"Mal\xC3\xAD" "cia", "Nina", "X", cuddles, RNB, {0x00AFFF}, 1, PURPLE, Rule::PlaylistMood},   // Malícia
        {"Faroeste Caboclo", "Legi\xC3\xA3o Urbana", "Que Pa\xC3\xADs \xC3\x89 Este", nullptr, 14, {0x0062FF}, 1,
         BLUE, Rule::GenreMood},
        {"Minha Felicidade", "Roberta Campos", "X", nullptr, 14, {0x00F3FF}, 1, BLUE, Rule::GenreMood},
        {"Redemption Song", "Bob Marley & The Wailers", "Uprising", nullptr, 20, {0xFFCC00, 0xFF1100}, 2, YELLOW,
         Rule::GenreMood},
        {"Creepin'", "Metro Boomin", "HEROES & VILLAINS", nullptr, RAP, {}, 0, RED, Rule::GenreMood},
    };
    check(cases, sizeof cases / sizeof cases[0]);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_fold_lowercases_and_drops_portuguese_accents);
    RUN_TEST(test_whole_words_only);
    RUN_TEST(test_logged_songs_in_the_neon_playlist);
    RUN_TEST(test_logged_songs_in_the_drive_playlist);
    RUN_TEST(test_logged_extras);
    RUN_TEST(test_replay_grey_covers_take_the_genres_colour_in_the_playlists_mood);
    RUN_TEST(test_the_genre_fallback_and_what_it_leaves_alone);
    RUN_TEST(test_picks_ignore_case_accents_and_suffixes_and_need_the_artist);
    RUN_TEST(test_a_pick_beats_a_colour_word_which_beats_the_mood);
    RUN_TEST(test_playlist_moods_by_word_and_emoji);
    RUN_TEST(test_the_description_counts_after_the_name);
    RUN_TEST(test_genre_moods);
    RUN_TEST(test_the_cover_chooses_inside_the_set);
    RUN_TEST(test_null_fields_are_empty);
    RUN_TEST(test_lights_send_the_swatchs_screen_colour_and_the_hours_cap);
    RUN_TEST(test_rule_names);
    RUN_TEST(test_tags_in_the_description);
    RUN_TEST(test_tags_beat_the_playlist_name_but_not_a_pick_or_the_title);
    RUN_TEST(test_picks_song_then_album_then_artist);
    RUN_TEST(test_near_hue_is_60_degrees);
    RUN_TEST(test_logged_songs_sc2);
    return UNITY_END();
}
