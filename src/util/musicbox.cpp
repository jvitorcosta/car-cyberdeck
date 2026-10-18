#include "musicbox.h"
#include <cmath>

namespace musicbox {

static constexpr int ATTACK = RATE / 500;              // 2 ms: no click
static constexpr float DECAY = 0.99986f;               // per sample: ~0.45 s to 1/e, a comb's ring
static constexpr float GAIN = 6000.0f;                 // per voice: 4 voices x 1.3 stays under 32767
static constexpr float OVERTONE = 0.3f;                // the octave above

float freq(int midi) { return 440.0f * powf(2.0f, (float)(midi - 69) / 12.0f); }

int nextTune(int count, int last, uint32_t rnd) {
    if (count <= 1) return 0;
    if (last < 0 || last >= count) return (int)(rnd % (uint32_t)count);
    int t = (int)(rnd % (uint32_t)(count - 1));
    return t >= last ? t + 1 : t;
}

// One sine period, 256 steps (built once).
static const float* table() {
    static float t[256];
    static bool built = false;
    if (!built) {
        for (int i = 0; i < 256; ++i) t[i] = sinf((float)i * 6.2831853f / 256.0f);
        built = true;
    }
    return t;
}

Player::Player(const char* text, size_t len, uint32_t seed)
    : text_(text), len_(len), count_(tunes::count(text, len)), rnd_(seed ? seed : 1),
      reader_(text, len, -1) {
    table();
    if (count_ > 0) startTune();
}

void Player::startTune() {
    rnd_ ^= rnd_ << 13; rnd_ ^= rnd_ >> 17; rnd_ ^= rnd_ << 5;   // xorshift32
    tune_ = nextTune(count_, tune_, rnd_);
    reader_ = tunes::Reader(text_, len_, tune_);
    resting_ = false;
    left_ = 0;
}

void Player::advance() {
    if (resting_) startTune();
    tunes::Note n;
    if (!reader_.next(&n)) {   // the tune is over: let it ring out, then the next
        resting_ = true;
        left_ = REST_MS * RATE / 1000;
        return;
    }
    if (n.midi != tunes::REST) {
        Voice& v = v_[next_];
        next_ = (next_ + 1) % VOICES;
        v.phase = 0;
        v.inc = (uint32_t)((double)freq(n.midi) * 4294967296.0 / RATE);
        v.amp = 1.0f;
        v.attack = ATTACK;
    }
    left_ = (uint32_t)n.ms * RATE / 1000;
    if (left_ == 0) left_ = 1;
}

void Player::stop() {
    if (stopping_) return;
    stopping_ = true;
    fade_ = FADE_MS * RATE / 1000;
}

size_t Player::fill(int16_t* out, size_t n) {
    if (count_ == 0) return 0;
    const float* t = table();
    size_t k = 0;
    for (; k < n; ++k) {
        if (stopping_ && fade_ == 0) break;
        if (left_ == 0) advance();
        --left_;
        float s = 0;
        for (Voice& v : v_) {
            if (v.amp < 1e-4f) continue;
            float env = v.amp;
            if (v.attack > 0) env *= (float)(ATTACK - v.attack + 1) / ATTACK, --v.attack;
            s += env * (t[v.phase >> 24] + OVERTONE * t[(v.phase >> 23) & 255]);
            v.amp *= DECAY;
            v.phase += v.inc;
        }
        s *= GAIN;
        if (stopping_) s *= (float)fade_-- / (float)(FADE_MS * RATE / 1000);
        out[k] = (int16_t)s;
    }
    return k;
}

}
