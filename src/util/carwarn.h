#pragma once
#include <cstdint>
// When she speaks the gauge cluster's warnings (coolant hot, battery low; obdplan's rules): once a
// warning has held for HOLD_MS (the battery dips below every limit while cranking), and again only
// after it has been clear for REARM_MS. The cluster itself still inverts the value at once. PURE,
// host-tested.
namespace carwarn {
enum class Warn : uint8_t { None, CoolantHot, BatteryLow };
constexpr uint32_t HOLD_MS = 10000;
constexpr uint32_t REARM_MS = 60000;

class Watch {
public:
    // hot / low: obdplan::coolantWarn / batteryWarn right now. At most one warning per call
    // (coolant first; the other follows on a later call).
    Warn update(bool hot, bool low, uint32_t nowMs) {
        const bool sayHot = coolant_.step(hot, nowMs);
        if (sayHot) return Warn::CoolantHot;
        if (battery_.step(low, nowMs)) return Warn::BatteryLow;
        return Warn::None;
    }
private:
    struct Cond {
        bool on = false, said = false, clearing = false;
        uint32_t since = 0, clearSince = 0;
        // True once, when the condition has held for HOLD_MS.
        bool step(bool c, uint32_t now) {
            if (c) {
                if (!on) { on = true; since = now; }
                clearing = false;
                if (!said && now - since >= HOLD_MS) { said = true; return true; }
                return false;
            }
            on = false;
            if (said) {
                if (!clearing) { clearing = true; clearSince = now; }
                if (now - clearSince >= REARM_MS) said = clearing = false;
            }
            return false;
        }
    };
    Cond coolant_, battery_;
};
}
