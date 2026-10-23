#pragma once
#include <cstdint>
// When the S3's music box (audio/greeting's tunes on "Waiting for a song...") starts and stops.
// It holds the audio driver and a 4 KB task, so only with Spotify's TLS memory to spare: it starts
// with startNeed bytes in the largest block, stops below keepNeed, and then rests RETRY_MS. A
// start whose audio didn't open (the codec not answering) rests RETRY_MS too, instead of a new
// task, I2S install and codec reset on every pass. PURE, host-tested.
namespace tunegate {
constexpr uint32_t RETRY_MS = 60000;
enum class Act : uint8_t { None, Play, Stop, AudioFailed };   // AudioFailed: rest (log it once)

struct In {
    bool want;            // "Waiting for a song..." is up and the tunes aren't muted
    bool playing;         // the music box runs
    bool busy;            // another sound: a howl, or the last tune still fading
    uint32_t failures;    // greeting::tunesFailures(): starts whose audio didn't open, since boot
    unsigned largest;     // mem::byteLargest()
    unsigned startNeed, keepNeed;
};

class Gate {
public:
    Act update(const In& in, uint32_t nowMs) {
        const bool newFailure = in.failures != seenFailures_;
        seenFailures_ = in.failures;
        if (!in.want) {
            resting_ = false;
            return in.playing ? Act::Stop : Act::None;
        }
        if (in.playing) {
            if (in.largest >= in.keepNeed) return Act::None;
            rest(nowMs);
            return Act::Stop;
        }
        if (newFailure) {
            rest(nowMs);
            return Act::AudioFailed;
        }
        if (resting_ && nowMs - since_ < RETRY_MS) return Act::None;   // wrap-safe
        if (in.busy) return Act::None;
        if (in.largest < in.startNeed) {
            rest(nowMs);
            return Act::None;
        }
        resting_ = false;
        return Act::Play;
    }
private:
    void rest(uint32_t nowMs) { resting_ = true; since_ = nowMs; }
    bool resting_ = false;
    uint32_t since_ = 0, seenFailures_ = 0;
};
}
