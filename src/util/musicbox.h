#pragma once
#include <cstddef>
#include <cstdint>
#include "tunes.h"
// The music box behind "Nothing playing" (S3): plays the tune set (util/tunes) as plucked
// music-box notes, synthesised while it plays (no clip in flash). A sine with a soft octave
// overtone, a 2 ms attack and a ringing decay, on a few voices so a note rings on under the
// next. One tune, a REST_MS pause, then another at random (never the same twice), for ever
// until stop(). PURE, host-tested: the clip task (audio/greeting) pulls samples in chunks.
namespace musicbox {
constexpr int RATE = 16000;            // audiosink clip rate
constexpr uint32_t REST_MS = 3000;     // between tunes
constexpr uint32_t FADE_MS = 300;      // stop()'s fade-out

float freq(int midi);
// The tune after `last` (-1: none yet) out of count, from a random number.
int nextTune(int count, int last, uint32_t rnd);

class Player {
public:
    Player(const char* text, size_t len, uint32_t seed);
    // Writes up to n samples; returns how many (0 once stopped and faded out, or with no tunes).
    size_t fill(int16_t* out, size_t n);
    void stop();                       // fade out over FADE_MS, then end
    int tune() const { return tune_; } // the tune playing (or resting after)
private:
    static constexpr int VOICES = 4;
    struct Voice { uint32_t phase = 0, inc = 0; float amp = 0; int attack = 0; };
    void startTune();
    void advance();                    // the next note, rest or tune
    const char* text_;
    size_t len_;
    int count_;
    uint32_t rnd_;
    int tune_ = -1;
    int next_ = 0;                     // round-robin voice
    Voice v_[VOICES];
    uint32_t left_ = 0;                // samples until the next event
    bool resting_ = false;
    tunes::Reader reader_;
    bool stopping_ = false;
    uint32_t fade_ = 0;
};
}
