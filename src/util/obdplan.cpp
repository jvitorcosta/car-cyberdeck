#include "obdplan.h"

namespace obdplan {
namespace {
constexpr obd::Pid SLOW[] = {obd::Pid::Coolant, obd::Pid::Load, obd::Pid::Intake, obd::Pid::Throttle,
                             obd::Pid::Battery};
constexpr int SLOW_N = (int)(sizeof SLOW / sizeof SLOW[0]);
}

bool Plan::blocked(obd::Pid p, uint32_t nowMs) const {
    return noData_[(int)p] && nowMs - noDataAt_[(int)p] < RETRY_MS;
}

obd::Pid Plan::next(uint32_t nowMs) {
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
    for (int k = 0; k < 2; ++k) {                        // RPM and speed take turns
        const obd::Pid p = fastSpeed_ ? obd::Pid::Speed : obd::Pid::Rpm;
        fastSpeed_ = !fastSpeed_;
        if (!blocked(p, nowMs)) return p;
    }
    return obd::Pid::Battery;   // everything else unsupported: the adapter's own reading
}

void Plan::done(obd::Pid p, obd::Reply r, uint32_t nowMs) {
    if (r == obd::Reply::NoData) {
        noData_[(int)p] = true;
        noDataAt_[(int)p] = nowMs;
    } else if (r == obd::Reply::Ok) {
        noData_[(int)p] = false;
    }
}

Link Health::reply(obd::Reply r) {
    if (r == obd::Reply::Error) {
        ++errors_;
        return state_;
    }
    errors_ = 0;
    state_ = r == obd::Reply::NoEngine ? Link::NoEngine : Link::Live;
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

float frac(float v, float lo, float hi) {
    if (!(hi > lo)) return 0.0f;
    const float f = (v - lo) / (hi - lo);
    return f < 0.0f ? 0.0f : f > 1.0f ? 1.0f : f;
}
}
