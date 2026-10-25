#pragma once
#include <cstddef>
#include <cstdint>
// Beats, energy and brightness out of the black deck's onboard mic (ES8311, weak: a hum floor of RMS
// ~400-750, mostly low frequency), for the music-driven LED mode. PURE, host-tested, no heap: the
// state is fixed-size so it runs on the MCU. Everything is measured against an adaptive noise floor,
// so the hum itself never counts as music.
namespace musicfeat {
constexpr int RATE = 16000;

// Bands (biquads, float): bass band-pass, mid band-pass, treble high-pass.
constexpr float BASS_LO_HZ = 70.0f, BASS_HI_HZ = 180.0f;
constexpr float MID_LO_HZ = 300.0f, MID_HI_HZ = 2000.0f;
constexpr float TREBLE_HZ = 3000.0f;

constexpr int HOP = 512;                      // analysis hop: 32 ms
constexpr float HOP_S = 0.032f;
constexpr int WARMUP_HOPS = 8;                // filters settling; the floor starts from this hop

// Noise floor per band: the band power's lower envelope; held while beats are coming.
constexpr float FLOOR_RISE_S = 30.0f, FLOOR_FALL_S = 2.0f;
constexpr uint32_t FLOOR_HOLD_MS = 2000;

// Beat: bass flux (power rise per hop) over a multiple of its 1 s median, bass power over a multiple
// of its floor, then a refractory.
constexpr int FLUX_RING = 31;                 // ~1 s of hops
constexpr float BEAT_FLUX_X = 1.6f, BEAT_POWER_X = 2.0f;
constexpr uint32_t REFRACTORY_MS = 250;
constexpr float FLUX_PEAK_DECAY_S = 10.0f;    // strength = flux / rolling flux peak

// Energy: total power above the floor, smoothed, over a rolling peak (never below a multiple of the floor).
constexpr float ENERGY_ATTACK_S = 0.05f, ENERGY_RELEASE_S = 0.4f;
constexpr float ENERGY_PEAK_DECAY_S = 10.0f, ENERGY_PEAK_MIN_X = 2.0f;
constexpr float BRIGHT_SMOOTH_S = 0.2f;

// Present: energy over PRESENT_ENERGY for PRESENT_NEED_HOPS of the last PRESENT_HOPS (1 s of 3 s).
constexpr int PRESENT_ENERGY = 25;
constexpr int PRESENT_HOPS = 94, PRESENT_NEED_HOPS = 31;

struct Features { bool beat; uint8_t strength, energy, brightness; bool present; };

class Analyzer {
public:
    Analyzer();
    // Feed mono 16 kHz samples (any block size); nowMs advances with the samples' end time.
    void feed(const int16_t* s, size_t n, uint32_t nowMs);
    // The features since the last take(): beat = at least one beat happened (strength = the strongest, 0..255);
    // energy / brightness / present as of now.
    Features take();
private:
    struct Biquad { float b0, b1, b2, a1, a2, z1, z2; };
    static void bandPass(Biquad& q, float loHz, float hiHz);
    static void highPass(Biquad& q, float hz);
    static float run(Biquad& q, float x);
    void hop(uint32_t hopMs);

    Biquad bass_, mid_, treble_;
    float sum_[3];                 // power sums over the hop in progress
    int fill_;                     // samples in that hop
    int hops_;                     // hops done (warm-up counter)
    float floor_[3];
    float prevBass_;
    float flux_[FLUX_RING];
    int fluxPos_;
    float fluxPeak_;
    bool everBeat_;
    uint32_t lastBeatMs_;
    bool anyBeat_;                 // a beat since the last take()
    uint8_t bestStrength_;
    float eSmooth_, ePeak_, bright_;
    uint8_t energy_, brightness_;
    bool presentRing_[PRESENT_HOPS];
    int presentPos_;
};
}
