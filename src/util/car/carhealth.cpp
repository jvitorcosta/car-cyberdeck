#include "carhealth.h"
#include <cstdio>
// See carhealth.h. PURE, host-tested.
namespace carhealth {
namespace {
void set(Verdict& v, Level l, const char* why) {
    v.level = l;
    snprintf(v.why, sizeof v.why, "%s", why);
}
// Keep the worse of two verdicts; on a tie the first one stays.
void worst(Verdict& into, const Verdict& v) {
    if (v.level > into.level) into = v;
}
bool hot(const Reading& r) { return !std::isnan(r.coolantC) && r.coolantC > COOLANT_HOT_C; }
}  // namespace

void Watch::feed(const Reading& r) {
    last_ = r;
    haveLast_ = true;
    // Under 12 V only means something while the engine runs (cranking and ignition-only dip lower),
    // and it has to last: smart charging idles at ~12.3 V, a real fault sits low for a minute.
    bool low = r.engineRunning && !std::isnan(r.volts) && r.volts < BATT_LOW_V;
    if (!low) {
        lowActive_ = false;
    } else if (!lowActive_) {
        lowActive_ = true;
        lowSinceMs_ = r.nowMs;
    }
    if (r.engineRunning && !std::isnan(r.ltftPct)) {
        ltftSum_ += r.ltftPct;
        ltftN_++;
    }
}

Verdict Watch::item(Item i) const {
    Verdict v;
    if (!haveLast_) return v;
    if (i == Item::Battery) {
        if (lowActive_ && last_.nowMs - lowSinceMs_ >= BATT_HOLD_MS) set(v, Level::Problem, "BATTERY LOW WHILE RUNNING");
        return v;
    }
    if (i != Item::Engine) return v;
    char buf[48];
    if (hot(last_)) {
        snprintf(buf, sizeof buf, "ENGINE HOT %dC", static_cast<int>(std::lround(last_.coolantC)));
        set(v, Level::Problem, buf);
    }
    Verdict m;
    if (last_.mil) {
        set(m, Level::Problem, "CHECK ENGINE LIGHT ON");
    } else if (last_.storedCodes > 0) {
        snprintf(buf, sizeof buf, "%d STORED CODE%s", last_.storedCodes, last_.storedCodes == 1 ? "" : "S");
        set(m, Level::Problem, buf);
    }
    worst(v, m);
    if (last_.pendingCodes == 1) {
        Verdict p;
        set(p, Level::Note, "1 PENDING CODE, NOT CONFIRMED");
        worst(v, p);
    } else if (last_.pendingCodes > 1) {
        Verdict p;
        snprintf(buf, sizeof buf, "%d PENDING CODES, NOT CONFIRMED", last_.pendingCodes);
        set(p, Level::Note, buf);
        worst(v, p);
    }
    worst(v, ltftVerdict_);
    return v;
}

bool Watch::critical() const { return haveLast_ && (last_.mil || hot(last_)); }

void Watch::endTrip() {
    if (ltftN_ > 0) {
        float mean = ltftSum_ / static_cast<float>(ltftN_);
        float a = std::fabs(mean);
        Verdict v;
        if (a > LTFT_NOTE) {
            char buf[48];
            snprintf(buf, sizeof buf, "FUEL TRIM %+d%% THIS TRIP", static_cast<int>(std::lround(mean)));
            set(v, a > LTFT_PROBLEM ? Level::Problem : Level::Note, buf);
        }
        ltftVerdict_ = v;
    }
    ltftSum_ = 0.0f;
    ltftN_ = 0;
}
}  // namespace carhealth
