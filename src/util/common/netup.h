#pragma once
#include <cstdint>
// When the WiFi link can carry traffic: connected AND holding an address. The driver can report
// connected with 0.0.0.0 (seen after an auto-reconnect to the hotspot: 40 s of failed DNS), and
// treating that as offline lets net::loop() run a clean reconnect. PURE, host-tested.
namespace netup {
inline bool usable(bool connected, uint32_t ip) { return connected && ip != 0; }

// When the 5-s link check should reconnect: at once on a real drop; for "connected without an
// address" only on the second check in a row (one such reading can be a transient, and a
// reconnect costs seconds).
class Watch {
public:
    bool reconnect(bool connected, uint32_t ip) {
        if (!connected) {
            noAddr_ = false;
            return true;
        }
        if (ip != 0) {
            noAddr_ = false;
            return false;
        }
        if (!noAddr_) {
            noAddr_ = true;
            return false;
        }
        noAddr_ = false;
        return true;
    }

private:
    bool noAddr_ = false;
};

// Reconnect pacing on the yellow deck (net::loop). A reconnect (net::connectAny) is ~16-24 s of
// joins, each with a full scan, and the radio is shared with the lights' BLE links: run back to
// back while no network was in range, they kept the radio from BLE nearly all the time. So each
// failed reconnect waits longer before the next: 5 s, 15 s, 30 s, then every 60 s (at most ~30 %
// of the radio's time); online again resets it.
class Rejoin {
public:
    static constexpr uint32_t WAITS_MS[] = {5000, 15000, 30000, 60000};
    static constexpr uint32_t STEPS = sizeof(WAITS_MS) / sizeof(WAITS_MS[0]);
    bool due(uint32_t nowMs) const { return fails_ == 0 || nowMs - at_ >= waitMs(); }   // wrap-safe
    void onResult(bool online, uint32_t nowMs) {
        if (online) { fails_ = 0; return; }
        if (fails_ < STEPS) ++fails_;
        at_ = nowMs;
    }
    uint32_t waitMs() const { return fails_ == 0 ? 0 : WAITS_MS[fails_ - 1]; }

private:
    uint32_t fails_ = 0, at_ = 0;
};
}
