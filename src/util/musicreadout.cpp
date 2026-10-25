#include "musicreadout.h"
#include <cstdio>

namespace musicreadout {

void Beats::reset() { *this = Beats(); }

void Beats::onPacket(uint32_t now, uint16_t seq, bool beat) {
    if (started_ && seq == lastSeq_) return;  // the same packet again
    if (!started_ || now - lastMs_ >= SIGNAL_MS) {
        reset();
        started_ = true;
        startMs_ = now;
    }
    lastMs_ = now;
    lastSeq_ = seq;
    if (beat && !lastBeat_) {
        at_[head_] = now;
        head_ = (head_ + 1) % RING;
        if (n_ < RING) n_++;
    }
    lastBeat_ = beat;
}

int Beats::bpm(uint32_t now) const {
    if (!started_) return -1;
    const uint32_t listened = now - startMs_;
    if (listened < MIN_MS) return -1;
    const uint32_t window = listened < WINDOW_MS ? listened : WINDOW_MS;
    uint32_t count = 0;  // while filling, every beat since the start (none is older)
    for (int i = 0; i < n_; i++) {
        if (now - at_[(head_ - 1 - i + RING) % RING] < WINDOW_MS) count++;
    }
    return static_cast<int>((count * 60000u + window / 2) / window);
}

State state(bool heard, uint32_t lastAt, bool present, uint32_t now) {
    if (!heard || now - lastAt >= SIGNAL_MS) return State::NoSignal;
    return present ? State::Music : State::Listening;
}

int level(uint8_t energy) { return (energy * 100 + 127) / 255; }

void format(State s, int bpm, uint8_t energy, char* out, size_t n) {
    if (!out || n == 0) return;
    if (s == State::NoSignal) {
        snprintf(out, n, ">NO SIGNAL");
    } else if (s == State::Listening) {
        snprintf(out, n, ">LISTENING");
    } else if (bpm < 0) {
        snprintf(out, n, ">MUSIC -- BPM  LVL %d%%", level(energy));
    } else {
        snprintf(out, n, ">MUSIC %d BPM  LVL %d%%", bpm > 999 ? 999 : bpm, level(energy));
    }
}

}  // namespace musicreadout
