#pragma once
// The yellow deck's music-mode readout on the dashboard's messages row, from the black deck's
// FeatPkts (util/musicwire): ">MUSIC 120 BPM  LVL 70%", ">LISTENING" (packets come, no music in
// them) or ">NO SIGNAL" (no packets). The yellow deck counts the beats itself, over the last 15 s.
// PURE, host-tested.
#include <cstddef>
#include <cstdint>

namespace musicreadout {
constexpr uint32_t WINDOW_MS = 15000;  // beats/min over this long
constexpr uint32_t MIN_MS = 3000;      // listened less than this: no rate yet ("--")
constexpr uint32_t SIGNAL_MS = 3000;   // no FeatPkt for this long: NO SIGNAL (and the count starts over)
constexpr int RING = 64;               // the black deck's 250 ms refractory: <= 60 beats in WINDOW_MS

// The beats of the last WINDOW_MS, counted on the FeatPkts' beat flag.
class Beats {
public:
    void reset();
    // A FeatPkt arrived at `now`: a beat on the flag's rising edge (one per packet at most; a repeat of
    // the last seq counts nothing). The first packet, or one after SIGNAL_MS without any, starts over.
    void onPacket(uint32_t now, uint16_t seq, bool beat);
    // Beats/min over the last WINDOW_MS, or over the time listened while that is shorter (no
    // undercount while the window fills); -1 before MIN_MS of listening.
    int bpm(uint32_t now) const;

private:
    uint32_t at_[RING] = {};
    int head_ = 0, n_ = 0;
    bool started_ = false, lastBeat_ = false;
    uint32_t startMs_ = 0, lastMs_ = 0;
    uint16_t lastSeq_ = 0;
};

enum class State : uint8_t { NoSignal, Listening, Music };
// heard: a FeatPkt came this run, the latest at lastAt with its present flag.
State state(bool heard, uint32_t lastAt, bool present, uint32_t now);
// energy 0..255 as a rounded percentage.
int level(uint8_t energy);
// The row's text ("--" for a bpm < 0), cut to n bytes.
void format(State s, int bpm, uint8_t energy, char* out, size_t n);
}  // namespace musicreadout
