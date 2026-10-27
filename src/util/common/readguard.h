#pragma once
#include <cstdint>
// Time limits for reading an HTTP body on the network task. Two clocks: a stall clock that
// restarts on every byte (the body stopped arriving) and an overall deadline (a server that
// trickles a byte at a time never stalls, but must not hold the polls up). millis() wrap-safe.
// PURE, host-tested.
namespace readguard {
enum class State { Ok, Stalled, Deadline };

class Guard {
public:
    Guard(uint32_t nowMs, uint32_t stallMs, uint32_t deadlineMs)
        : start_(nowMs), last_(nowMs), stallMs_(stallMs), deadlineMs_(deadlineMs) {}
    void onData(uint32_t nowMs) { last_ = nowMs; }
    State check(uint32_t nowMs) const {
        if (nowMs - start_ > deadlineMs_) return State::Deadline;
        if (nowMs - last_ > stallMs_) return State::Stalled;
        return State::Ok;
    }
private:
    uint32_t start_, last_, stallMs_, deadlineMs_;
};
}
