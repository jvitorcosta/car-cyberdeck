#pragma once
#include <cstdint>
#include "obd.h"
// The old deck's OBD gauges: which request goes next, whether the adapter is still there, when
// the gauges show, and the bars' colours. PURE, host-tested.
// Spec: docs/superpowers/specs/2026-10-15-obd-gauges-design.md
namespace obdplan {
constexpr uint32_t SLOW_MS = 2000;     // one slow value per this long; RPM and speed otherwise
constexpr uint32_t RETRY_MS = 30000;   // a PID the car doesn't support is asked again after this
constexpr uint32_t HOLD_MS = 5000;     // the gauges stay this long after the link leaves Live
constexpr int LOST_AFTER = 3;          // failed requests in a row: the adapter is gone

enum class Link : uint8_t { Off, Connecting, NoEngine, Live };

class Plan {
public:
    obd::Pid next(uint32_t nowMs);
    void done(obd::Pid p, obd::Reply r, uint32_t nowMs);
private:
    bool blocked(obd::Pid p, uint32_t nowMs) const;
    bool started_ = false, fastSpeed_ = false;
    uint8_t slowNext_ = 0;
    uint32_t lastSlowMs_ = 0;
    bool noData_[(int)obd::Pid::COUNT] = {};
    uint32_t noDataAt_[(int)obd::Pid::COUNT] = {};
};

// Ok / NoData: Live (the car answered). NoEngine: NoEngine. Error: counted; LOST_AFTER in a row
// and lost() is true (the state stays as it was).
class Health {
public:
    Link reply(obd::Reply r);
    bool lost() const { return errors_ >= LOST_AFTER; }
    Link state() const { return state_; }
private:
    Link state_ = Link::Connecting;
    int errors_ = 0;
};

// Gauges on screen: at once when live, until live has been false for HOLD_MS.
class Hold {
public:
    bool update(bool live, uint32_t nowMs);
private:
    bool shown_ = false;
    uint32_t lastLiveMs_ = 0;
};

enum class Band : uint8_t { Cold, Ok, Warn, Hot };
Band rpmBand(float rpm, float redline);   // Ok < 70 % <= Warn < 90 % <= Hot
Band coolantBand(float c);                // Cold < 60 C, Ok <= 105 C, else Hot
Band batteryBand(float v);                // Hot < 11.8 V, Warn < 12.2 V, else Ok
float frac(float v, float lo, float hi);  // clamped to 0..1; 0 for an empty range
}
