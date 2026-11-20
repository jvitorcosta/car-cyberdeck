#include "drivelog.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace drivelog {
namespace {
int16_t q(float v, float scale) {
    if (std::isnan(v)) return NONE;
    const float s = std::round(v * scale);
    if (s <= -32767.0f || s >= 32767.0f) return NONE;   // out of any real range: missing, not clipped
    return static_cast<int16_t>(s);
}

// A cell: "", or the value with `dp` decimals from its scaled int.
int cell(char* out, size_t n, int16_t v, int scale, int dp) {
    if (v == NONE) return snprintf(out, n, ",");
    return snprintf(out, n, ",%.*f", dp, static_cast<double>(v) / scale);
}
}  // namespace

const char HEADER[] = "t_utc,ms,kmh,rpm,coolant_c,iat_c,throttle_pct,pedal_pct,load_pct,volt,nv\n";

Row pack(uint32_t ms, const Sample& s) {
    Row r;
    r.ms = ms;
    r.kmh10 = q(s.kmh, 10.0f);
    r.rpm = q(s.rpm, 1.0f);
    r.coolant10 = q(s.coolantC, 10.0f);
    r.iat10 = q(s.iatC, 10.0f);
    r.throttle10 = q(s.throttlePct, 10.0f);
    r.pedal10 = q(s.pedalPct, 10.0f);
    r.load10 = q(s.loadPct, 10.0f);
    r.volt100 = q(s.volts, 100.0f);
    return r;
}

float nv(float rpm, float kmh) {
    if (std::isnan(rpm) || std::isnan(kmh) || kmh < NV_MIN_KMH || !(rpm > 0.0f)) return NAN;
    return rpm / kmh;
}

size_t csvLine(const Row& r, double unixT, char* out, size_t n) {
    if (!out || n == 0) return 0;
    char buf[128];
    int len = unixT > 0 ? snprintf(buf, sizeof buf, "%.1f,%lu", unixT, static_cast<unsigned long>(r.ms))
                        : snprintf(buf, sizeof buf, ",%lu", static_cast<unsigned long>(r.ms));
    len += cell(buf + len, sizeof buf - len, r.kmh10, 10, 1);
    len += cell(buf + len, sizeof buf - len, r.rpm, 1, 0);
    len += cell(buf + len, sizeof buf - len, r.coolant10, 10, 1);
    len += cell(buf + len, sizeof buf - len, r.iat10, 10, 1);
    len += cell(buf + len, sizeof buf - len, r.throttle10, 10, 1);
    len += cell(buf + len, sizeof buf - len, r.pedal10, 10, 1);
    len += cell(buf + len, sizeof buf - len, r.load10, 10, 1);
    len += cell(buf + len, sizeof buf - len, r.volt100, 100, 2);
    const float ratio = nv(r.rpm == NONE ? NAN : static_cast<float>(r.rpm),
                           r.kmh10 == NONE ? NAN : static_cast<float>(r.kmh10) / 10.0f);
    len += std::isnan(ratio) ? snprintf(buf + len, sizeof buf - len, ",\n")
                             : snprintf(buf + len, sizeof buf - len, ",%.1f\n", static_cast<double>(ratio));
    if (len <= 0 || static_cast<size_t>(len) >= n) {
        out[0] = '\0';
        return 0;
    }
    memcpy(out, buf, static_cast<size_t>(len) + 1);
    return static_cast<size_t>(len);
}

double rowUnix(int64_t nowUnix, uint32_t nowMs, uint32_t rowMs) {
    return static_cast<double>(nowUnix) - static_cast<double>(nowMs - rowMs) / 1000.0;
}

void Ring::push(const Row& r) {
    if (count_ == CAP) {             // full: the oldest goes
        head_ = (head_ + 1) % CAP;
        --count_;
        ++dropped_;
    }
    rows_[(head_ + count_) % CAP] = r;
    ++count_;
    ++next_;
}

const Row& Ring::at(uint32_t i) const { return rows_[(head_ + i) % CAP]; }

void Ring::release(uint32_t upTo) {
    while (count_ && static_cast<int32_t>(firstSeq() - upTo) < 0) {
        head_ = (head_ + 1) % CAP;
        --count_;
    }
}

Route route(uint32_t ringRows, uint32_t spoolRows) {
    if (spoolRows == 0 && ringRows < SPOOL_AT) return Route::Ring;
    return spoolRows < SPOOL_MAX ? Route::Spool : Route::Drop;
}

uint32_t refill(uint32_t ringRows, uint32_t spoolRows) {
    if (spoolRows == 0 || ringRows >= BATCH) return 0;
    uint32_t k = spoolRows < BATCH ? spoolRows : BATCH;
    if (ringRows + k > SPOOL_AT) k = SPOOL_AT - ringRows;
    return k;
}

bool partDue(uint32_t pending, uint32_t oldestMs, uint32_t nowMs, uint32_t maxWaitMs) {
    if (pending == 0) return false;
    return pending >= BATCH || nowMs - oldestMs >= maxWaitMs;
}

bool tripName(int64_t unixStart, char* out, size_t n) {
    if (unixStart < 1577836800) return false;   // 2020-01-01: before it, the clock isn't set
    // days since 1970 to a civil date (H. Hinnant's civil_from_days): no gmtime, the same on the
    // deck and on the host
    const int64_t days = unixStart / 86400, secs = unixStart % 86400;
    const int64_t z = days + 719468, era = z / 146097, doe = z - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    const int d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1), m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    const int y = static_cast<int>(yoe + era * 400 + (m <= 2 ? 1 : 0));
    const int len = snprintf(out, n, "%04d%02d%02d-%02d%02d%02d", y, m, d, static_cast<int>(secs / 3600),
                             static_cast<int>(secs / 60 % 60), static_cast<int>(secs % 60));
    return len > 0 && static_cast<size_t>(len) < n;
}

bool appendRaw(char* out, size_t cap, const char* request, const char* raw) {
    size_t o = strlen(out);
    auto add = [&](char c) {
        if (o + 1 >= cap) return false;
        out[o++] = c;
        out[o] = '\0';
        return true;
    };
    for (const char* p = request; *p && *p != '\r'; ++p)
        if (!add(*p)) return false;
    for (const char* p = " -> "; *p; ++p)
        if (!add(*p)) return false;
    for (const char* p = raw ? raw : ""; *p; ++p) {
        const char c = *p == '\r' || *p == '\n' ? '|' : (*p >= 0x20 && *p < 0x7F ? *p : '?');
        if (!add(c)) return false;
    }
    return add('\n');
}

}  // namespace drivelog
