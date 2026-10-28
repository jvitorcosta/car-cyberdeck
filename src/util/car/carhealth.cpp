#include "carhealth.h"
#include <cstdio>
#include <cstring>
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
    // and it has to last: smart charging idles at ~12.3 V, a real fault sits low for a minute. A missed
    // reading (NAN: a Bluetooth hiccup) is not a recovery, so it keeps the state and the timer; only a
    // good reading or the engine stopping clears it.
    if (!r.engineRunning) {
        lowActive_ = false;
    } else if (!std::isnan(r.volts)) {
        if (r.volts >= BATT_LOW_V) {
            lowActive_ = false;
        } else if (!lowActive_) {
            lowActive_ = true;
            lowSinceMs_ = r.nowMs;
        }
    }
    if (!std::isnan(r.volts)) voltsSeen_ = true;
    if (lowActive_ && r.nowMs - lowSinceMs_ >= BATT_HOLD_MS) battLowSeen_ = true;
    if (r.engineRunning && !std::isnan(r.ltftPct)) {
        ltftSum_ += r.ltftPct;
        ltftN_++;
    }
}

Verdict Watch::item(Item i) const {
    Verdict v;
    if (i == Item::Battery) {
        if (lastBattLow_) set(v, Level::Note, "BATTERY WAS LOW LAST TRIP");  // history: a note, not a fault now
        if (haveLast_ && lowActive_ && last_.nowMs - lowSinceMs_ >= BATT_HOLD_MS) {
            Verdict p;
            set(p, Level::Problem, "BATTERY LOW WHILE RUNNING");
            worst(v, p);
        }
        v.known = voltsSeen_ || lastBattLow_ || v.level != Level::Ok;
        return v;
    }
    if (i != Item::Engine) return v;
    worst(v, ltftVerdict_);
    if (!haveLast_) {
        v.known = codesRead_ || ltftJudged_ || v.level != Level::Ok;
        return v;
    }
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
    v.known = codesRead_ || ltftJudged_ || v.level != Level::Ok;
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
        ltftJudged_ = true;
    }
    ltftSum_ = 0.0f;
    ltftN_ = 0;
    lastBattLow_ = battLowSeen_;
    battLowSeen_ = false;
}

void Watch::codesRead() { codesRead_ = true; }

Memo Watch::memo() const { return Memo{battLowSeen_, ltftSum_, ltftN_}; }

void Watch::restore(const Memo& m) {
    battLowSeen_ = m.battLowSeen;
    ltftSum_ = m.ltftSum;
    ltftN_ = m.ltftN;
}

void encode(const Memo& m, uint8_t out[MEMO_BYTES]) {
    uint32_t f;
    std::memcpy(&f, &m.ltftSum, 4);
    out[0] = m.battLowSeen ? 1 : 0;
    for (int i = 0; i < 4; i++) {
        out[1 + i] = static_cast<uint8_t>(f >> (8 * i));
        out[5 + i] = static_cast<uint8_t>(m.ltftN >> (8 * i));
    }
}

bool decode(const uint8_t* in, size_t len, Memo* m) {
    if (!in || !m || len != MEMO_BYTES || in[0] > 1) return false;
    uint32_t f = 0, n = 0;
    for (int i = 0; i < 4; i++) {
        f |= static_cast<uint32_t>(in[1 + i]) << (8 * i);
        n |= static_cast<uint32_t>(in[5 + i]) << (8 * i);
    }
    float sum;
    std::memcpy(&sum, &f, 4);
    if (!std::isfinite(sum)) return false;
    m->battLowSeen = in[0] == 1;
    m->ltftSum = sum;
    m->ltftN = n;
    return true;
}
}  // namespace carhealth
