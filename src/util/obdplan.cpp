#include "obdplan.h"

namespace obdplan {
namespace {
using obd::Pid;
constexpr Pid SLOW[] = {Pid::Coolant, Pid::Battery, Pid::Fuel, Pid::Intake, Pid::Ethanol, Pid::Load,
                        Pid::FuelSys};
constexpr int SLOW_N = (int)(sizeof SLOW / sizeof SLOW[0]);
// The mode 01 PID byte per Pid (0: the adapter's own reading, ATRV). Order matches obd::Pid.
constexpr uint8_t PID_BYTE[(int)Pid::COUNT] = {0x0C, 0x0D, 0x05, 0x04, 0x0F, 0x11, 0,
                                               0x49, 0x4A, 0x2F, 0x0B, 0x10, 0x52, 0x03};

bool isFast(Pid p) {
    return p == Pid::Rpm || p == Pid::Speed || p == Pid::PedalD || p == Pid::PedalE || p == Pid::Throttle ||
           p == Pid::Maf || p == Pid::Map;
}
}

bool Plan::blocked(Pid p, uint32_t nowMs) const {
    if (PID_BYTE[(int)p] && !obd::isSupported(supported_, PID_BYTE[(int)p])) return true;
    return noData_[(int)p] && nowMs - noDataAt_[(int)p] < RETRY_MS;
}

void Plan::setSupported(const uint32_t bits[4]) {
    for (int i = 0; i < 4; ++i) supported_[i] = bits[i];
}

Pid Plan::fastExtra(uint32_t nowMs) {
    for (int j = 0; j < 3; ++j) {
        const int which = (fastExtra_ + j) % 3;
        Pid p = Pid::Throttle;
        if (which == 0) p = blocked(Pid::PedalD, nowMs) ? Pid::PedalE : Pid::PedalD;
        else if (which == 2) p = blocked(Pid::Maf, nowMs) ? Pid::Map : Pid::Maf;
        if (!blocked(p, nowMs)) {
            fastExtra_ = (uint8_t)((which + 1) % 3);
            return p;
        }
    }
    return Pid::COUNT;
}

Pid Plan::next(uint32_t nowMs) {
    if (crank_) return ++crankN_ % 10 == 0 ? Pid::Rpm : Pid::Battery;
    if (!started_ || nowMs - lastSlowMs_ >= SLOW_MS) {   // a slow value's turn
        started_ = true;
        lastSlowMs_ = nowMs;
        for (int k = 0; k < SLOW_N; ++k) {
            const obd::Pid p = SLOW[(slowNext_ + k) % SLOW_N];
            if (!blocked(p, nowMs)) {
                slowNext_ = (uint8_t)((slowNext_ + k + 1) % SLOW_N);
                return p;
            }
        }
    }
    for (int k = 0; k < 3; ++k) {                        // RPM, speed, one extra
        const uint8_t step = fastStep_;
        fastStep_ = (uint8_t)((fastStep_ + 1) % 3);
        const Pid p = step == 0 ? Pid::Rpm : step == 1 ? Pid::Speed : fastExtra(nowMs);
        if (p != Pid::COUNT && !blocked(p, nowMs)) return p;
    }
    return Pid::Battery;   // everything else unsupported: the adapter's own reading
}

void Plan::done(obd::Pid p, obd::Reply r, uint32_t nowMs) {
    if (r == obd::Reply::NoData && p != obd::Pid::Rpm) {   // RPM's NO DATA: engine off, ask again
        noData_[(int)p] = true;
        noDataAt_[(int)p] = nowMs;
    } else if (r == obd::Reply::Ok) {
        noData_[(int)p] = false;
    }
}

Link Health::reply(obd::Pid p, obd::Reply r) {
    if (r == obd::Reply::Error) {
        ++errors_;
        return state_;
    }
    errors_ = 0;                                         // the adapter answered
    if (r == obd::Reply::NoEngine || (r == obd::Reply::NoData && p == obd::Pid::Rpm))
        state_ = Link::NoEngine;
    else if (r == obd::Reply::Ok && p != obd::Pid::Battery)
        state_ = Link::Live;
    return state_;
}

bool Hold::update(bool live, uint32_t nowMs) {
    if (live) {
        shown_ = true;
        lastLiveMs_ = nowMs;
    } else if (shown_ && nowMs - lastLiveMs_ > HOLD_MS) {
        shown_ = false;
    }
    return shown_;
}

Band rpmBand(float rpm, float redline) {
    const float f = frac(rpm, 0.0f, redline);
    return f < 0.7f ? Band::Ok : f < 0.9f ? Band::Warn : Band::Hot;
}
Band coolantBand(float c) { return c < 60.0f ? Band::Cold : c <= 105.0f ? Band::Ok : Band::Hot; }
Band batteryBand(float v) { return v < 11.8f ? Band::Hot : v < 12.2f ? Band::Warn : Band::Ok; }

uint32_t staleAfterMs(Pid p) { return isFast(p) ? STALE_FAST_MS : STALE_SLOW_MS; }

bool batteryWarn(float volts, bool engineRunning) { return volts < (engineRunning ? 13.0f : 12.2f); }
bool coolantWarn(float c) { return c > 105.0f; }
bool rpmWarn(float rpm, float redline) { return rpm >= 0.85f * redline; }

float frac(float v, float lo, float hi) {
    if (!(hi > lo)) return 0.0f;
    const float f = (v - lo) / (hi - lo);
    return f < 0.0f ? 0.0f : f > 1.0f ? 1.0f : f;
}
}
