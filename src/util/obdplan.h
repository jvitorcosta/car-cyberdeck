#pragma once
#include <cstdint>
#include "obd.h"
// The old deck's OBD gauges: which request goes next, whether the adapter is still there, when
// the gauges show, and the bars' colours. PURE, host-tested.
// Spec: docs/superpowers/specs/2026-10-15-obd-gauges-design.md
namespace obdplan {
constexpr uint32_t SLOW_MS = 2000;     // one slow value per this long; RPM and speed otherwise
constexpr uint32_t RETRY_MS = 30000;   // a PID the car doesn't support is asked again after this
                                       // (never RPM: its NO DATA means the engine is off)
constexpr uint32_t HOLD_MS = 5000;     // the gauges stay this long after the link leaves Live
constexpr int LOST_AFTER = 3;          // failed requests in a row: the adapter is gone

enum class Link : uint8_t { Off, Connecting, NoEngine, Live };

// The request order. Fast, repeating: RPM, speed, then one of pedal (D, else E), throttle, air
// (MAF, else MAP) in turn; one slow value (coolant, battery, fuel, intake, ethanol, load, fuel
// system) every SLOW_MS. What the car's scan says it lacks is never asked (the adapter's battery
// always is); before setSupported() everything counts as supported.
class Plan {
public:
    obd::Pid next(uint32_t nowMs);
    void done(obd::Pid p, obd::Reply r, uint32_t nowMs);
    void setSupported(const uint32_t bits[4]);
    // Waiting for the engine to start: only the battery (9 in 10) and RPM (first, then every 10th),
    // for the crank dip. Leaving it asks the battery once more straight away (the last reading is
    // the crank's, not the charging voltage).
    void crankWatch(bool on) {
        if (crank_ && !on) batteryNext_ = true;
        crank_ = on;
    }
private:
    bool blocked(obd::Pid p, uint32_t nowMs) const;   // unsupported, or backing off after NO DATA
    obd::Pid fastExtra(uint32_t nowMs);              // pedal / throttle / air in turn; COUNT if none
    bool started_ = false, crank_ = false, batteryNext_ = false;
    uint8_t slowNext_ = 0, fastStep_ = 0, fastExtra_ = 0, crankN_ = 0;
    uint32_t lastSlowMs_ = 0;
    uint32_t supported_[4] = {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu};
    bool noData_[(int)obd::Pid::COUNT] = {};
    uint32_t noDataAt_[(int)obd::Pid::COUNT] = {};
};

constexpr uint32_t STALE_FAST_MS = 3000, STALE_SLOW_MS = 30000;
// How old a reading may be before it's drawn as stale: STALE_FAST_MS for the fast set (RPM, speed,
// pedal, throttle, MAF/MAP), STALE_SLOW_MS for the slow ones (read only every ~14 s).
uint32_t staleAfterMs(obd::Pid p);
// Warnings, drawn inverted: battery under 13.0 V with the engine running (not charging) or under
// 12.2 V with it off; coolant over 105 C; RPM from 85 % of the redline.
bool batteryWarn(float volts, bool engineRunning);
bool coolantWarn(float c);
bool rpmWarn(float rpm, float redline);

// The link's state from each reply to request p. Live only from a mode 01 value: the adapter
// reads the battery (ATRV) with the ignition off too. NoEngine from NoEngine, and from NO DATA
// on RPM (mandatory on every OBD-II car: no answer means the engine isn't on). NO DATA on another
// PID only means unsupported. Error: counted; LOST_AFTER in a row and lost() is true (the state
// stays as it was).
class Health {
public:
    Link reply(obd::Pid p, obd::Reply r);
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

float frac(float v, float lo, float hi);  // clamped to 0..1; 0 for an empty range
}
