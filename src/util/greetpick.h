#pragma once
#include <cstdint>
// The S3 boots with one of its greetings at random: the original (data/greeting.pcm) or the
// alternative (data/greeting_alt.pcm, McQueen's "Eu sou a velocidade"). PURE, host-tested.
namespace greetpick {
// 0 = the original, 1 = the alternative; `rnd` is a random word (esp_random()).
inline int pick(uint32_t rnd, bool haveAlt) { return haveAlt ? (int)(rnd & 1u) : 0; }
}
