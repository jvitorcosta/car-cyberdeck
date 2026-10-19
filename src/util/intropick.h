#pragma once
#include <cstdint>
// The S3's boot opening, picked at random each boot (spec 2026-10-19-random-intro): the kiss
// video, the pixel Honda's night drive or the pixel McQueen's. Never last boot's (kept in NVS by
// main.cpp) and never one whose files the build lacks; the Honda is always there. PURE, host-tested.
namespace intropick {
enum Opening : uint8_t { KISS = 0, HONDA = 1, MCQUEEN = 2, COUNT = 3 };
enum Greeting : uint8_t { NONE = 0, MAIN = 1, ALT = 2 };   // data/greeting.pcm, greeting_alt.pcm

// last: the previous boot's opening (anything out of range = none).
Opening pick(uint32_t rnd, int last, bool haveKiss, bool haveMcQueen);
Greeting greetingFor(Opening o);   // kiss: none (the video has its sound)
const char* name(Opening o);       // "kiss", "honda", "mcqueen" (logs, INTRO_FORCE)
int parse(const char* s);          // name() back to an Opening; -1 otherwise
int forced(int f, bool haveKiss, bool haveMcQueen);   // f if it's an available opening, else -1
}
