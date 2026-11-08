#include "tripsum.h"
#include <cmath>
#include <cstdio>

namespace tripsum {

static constexpr uint32_t ROW_MS = 60000;
static constexpr uint32_t MAX_STEP_MS = 3000;
static constexpr uint32_t RECHARGE_MS = 300000;
static constexpr float CHARGING_V = 13.8f;

static float avgOf(double sum, uint32_t n) {
    return n ? static_cast<float>(sum / n) : NAN;
}

void Trip::add(const Sample& s) {
    if (!started_) {
        started_ = true;
        lastMs_ = s.nowMs;
        if (s.fresh && s.rpm > 0.0f) engineSeen_ = true;  // the window opens with the first running sample
        return;
    }
    uint32_t dt = s.nowMs - lastMs_;  // unsigned: survives the millis() wrap
    lastMs_ = s.nowMs;
    if (dt > MAX_STEP_MS) dt = MAX_STEP_MS;

    totalMs_ += dt;
    cur_.ms += dt;

    if (s.fresh) {
        cur_.freshN++;
        const bool kmhKnown = !std::isnan(s.kmh);
        if (kmhKnown) {
            const double km = static_cast<double>(s.kmh) * dt / 3600000.0;
            km_ += km;
            cur_.km += km;
            cur_.kmhSum += static_cast<double>(s.kmh);
            cur_.kmhN++;
            if (!cur_.hasMax || s.kmh > cur_.maxKmh) cur_.maxKmh = s.kmh;
            cur_.hasMax = true;
        }
        if (!std::isnan(s.rpm)) {
            cur_.rpmSum += static_cast<double>(s.rpm);
            cur_.rpmN++;
        }
        if (!std::isnan(s.litres)) {
            litres_ += static_cast<double>(s.litres);
            cur_.litres += static_cast<double>(s.litres);
        }
        if (kmhKnown && s.rpm > 0.0f && s.kmh < 1.0f) {  // NAN rpm compares false: not idle
            idleMs_ += dt;
            cur_.idleMs += dt;
        }
        if (!std::isnan(s.volts)) {
            cur_.vSum += static_cast<double>(s.volts);
            cur_.vN++;
            if (!cur_.hasV || s.volts < cur_.vMin) cur_.vMin = s.volts;
            if (!cur_.hasV || s.volts > cur_.vMax) cur_.vMax = s.volts;
            cur_.hasV = true;
        }
        if (!std::isnan(s.coolantC)) {
            if (!cur_.hasCool || s.coolantC > cur_.coolMax) cur_.coolMax = s.coolantC;
            cur_.hasCool = true;
        }
        if (!std::isnan(s.trimS)) {
            cur_.tsSum += static_cast<double>(s.trimS);
            cur_.tsN++;
        }
        if (!std::isnan(s.trimL)) {
            cur_.tlSum += static_cast<double>(s.trimL);
            cur_.tlN++;
        }
        if (s.rpm > 0.0f) engineSeen_ = true;
    }

    // Recharge window: 5 minutes of sample time from the first running sample; the step that crosses
    // the end only counts up to it.
    if (engineSeen_ && winMs_ < RECHARGE_MS) {
        uint32_t inWin = RECHARGE_MS - winMs_;
        if (inWin > dt) inWin = dt;
        winMs_ += inWin;
        if (s.fresh && !std::isnan(s.volts)) {
            if (!hasPeak_ || s.volts > peakV_) peakV_ = s.volts;
            hasPeak_ = true;
            if (s.volts > CHARGING_V) overMs_ += inWin;
        }
    }

    if (cur_.ms >= ROW_MS) closeRow();
}

void Trip::closeRow() {
    if (!cur_.freshN) {  // a minute of nothing read: a row of nulls would only fill the flash
        nextMinute_++;
        cur_ = Acc{};
        return;
    }
    Row r{};
    r.minute = nextMinute_++;
    r.km = static_cast<float>(cur_.km);
    r.avgKmh = avgOf(cur_.kmhSum, cur_.kmhN);
    r.maxKmh = cur_.hasMax ? cur_.maxKmh : NAN;
    r.avgRpm = avgOf(cur_.rpmSum, cur_.rpmN);
    r.litres = static_cast<float>(cur_.litres);
    r.idleS = static_cast<uint16_t>(cur_.idleMs / 1000);
    r.vMin = cur_.hasV ? cur_.vMin : NAN;
    r.vAvg = avgOf(cur_.vSum, cur_.vN);
    r.vMax = cur_.hasV ? cur_.vMax : NAN;
    r.coolMax = cur_.hasCool ? cur_.coolMax : NAN;
    r.trimS = avgOf(cur_.tsSum, cur_.tsN);
    r.trimL = avgOf(cur_.tlSum, cur_.tlN);
    r.flags = cur_.flags;
    row_ = r;
    ready_ = true;
    cur_ = Acc{};
}

Row Trip::takeRow() {
    ready_ = false;
    return row_;
}

Summary Trip::summary() const {
    Summary s{};
    s.km = static_cast<float>(km_);
    s.litres = static_cast<float>(litres_);
    s.kmL = litres_ > 0.0 ? static_cast<float>(km_ / litres_) : NAN;
    s.avgKmh = totalMs_ ? static_cast<float>(km_ / (totalMs_ / 3600000.0)) : NAN;
    s.seconds = totalMs_ / 1000;
    s.idleS = idleMs_ / 1000;
    return s;
}

Recharge Trip::recharge() const {
    Recharge r{};
    r.peakV = hasPeak_ ? peakV_ : NAN;
    r.secondsOver13_8 = static_cast<uint16_t>(overMs_ / 1000);
    r.done = winMs_ >= RECHARGE_MS;
    return r;
}

// "null" for unknown, else the number with the given decimals. JSON has no inf either, so anything
// non-finite is "null" (an "inf" would make the whole row unreadable on the PC).
static void num(char* buf, size_t n, float v, int decimals) {
    if (!std::isfinite(v)) snprintf(buf, n, "null");
    else snprintf(buf, n, "%.*f", decimals, static_cast<double>(v));
}

size_t jsonRow(const Row& r, const char* tripId, char* out, size_t n) {
    if (!out || n == 0) return 0;
    char id[24];
    size_t k = 0;
    for (const char* p = tripId; p && *p && k < sizeof(id) - 1; p++) {
        if (*p == '"' || *p == '\\' || static_cast<unsigned char>(*p) < 0x20) continue;
        id[k++] = *p;
    }
    id[k] = 0;

    char km[16], kmh[16], kmhMax[16], rpm[16], l[16], vLo[16], vAv[16], vHi[16], cool[16], ts[16], tl[16];
    num(km, sizeof(km), r.km, 2);
    num(kmh, sizeof(kmh), r.avgKmh, 1);
    num(kmhMax, sizeof(kmhMax), r.maxKmh, 0);
    num(rpm, sizeof(rpm), r.avgRpm, 0);
    num(l, sizeof(l), r.litres, 3);
    num(vLo, sizeof(vLo), r.vMin, 1);
    num(vAv, sizeof(vAv), r.vAvg, 1);
    num(vHi, sizeof(vHi), r.vMax, 1);
    num(cool, sizeof(cool), r.coolMax, 0);
    num(ts, sizeof(ts), r.trimS, 1);
    num(tl, sizeof(tl), r.trimL, 1);

    const int len = snprintf(out, n,
                             "{\"t\":\"%s\",\"m\":%u,\"km\":%s,\"kmh\":%s,\"kmhMax\":%s,\"rpm\":%s,\"l\":%s,"
                             "\"idle\":%u,\"v\":[%s,%s,%s],\"cool\":%s,\"trim\":[%s,%s],\"f\":%u}\n",
                             id, static_cast<unsigned>(r.minute), km, kmh, kmhMax, rpm, l,
                             static_cast<unsigned>(r.idleS), vLo, vAv, vHi, cool, ts, tl,
                             static_cast<unsigned>(r.flags));
    if (len < 0 || static_cast<size_t>(len) >= n) {
        out[0] = 0;
        return 0;
    }
    return static_cast<size_t>(len);
}

}  // namespace tripsum
