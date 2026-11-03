#pragma once
#include <cstdint>
// The Japanese greeting the Honda's night drive shows in the sky, beside the boot terminal
// (ui/bootscene): kana/kanji in Unifont (data/cjk16.bin), the romaji and the English under them in
// the terminal's font. One at random each boot, never last boot's (kept in NVS by main.cpp); the
// time-of-day ones only once the clock is known (after a soft reset; a cold boot counts from 1970).
// PURE, host-tested (every glyph is checked against data/cjk16.bin: no boxes on screen).
namespace greetpick {
enum When : uint8_t { ANY = 0, MORNING, DAY, EVENING, NIGHT };
struct Greeting {
    const char16_t* jp;     // kana/kanji, all 16 px wide in Unifont
    const char* romaji;     // upper-case ASCII, as the BIOS log
    const char* english;
};
// What fits the sky right of the terminal (x 214-318): 6 glyphs of 16 px, 17 characters of 6 px.
constexpr int MAX_GLYPHS = 6, MAX_LATIN = 17;

int count();
const Greeting& at(int i);   // i in [0, count())
When whenOf(int i);
// The part of the day for a local minute of the day (0..1439): 5-11 h morning, 11-18 h day,
// 18-22 h evening, 22-5 h night.
When slot(int minuteOfDay);
// minuteOfDay < 0: the clock isn't set, so only the ANY greetings. last: the previous boot's
// (anything out of range = none).
int pick(uint32_t rnd, int last, int minuteOfDay);
}
