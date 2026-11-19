#include "obdwire.h"
#include <cstdio>
#include <cstring>
#include "../common/jsonflat.h"

namespace obdwire {
namespace {
using obd::Pid;

// One "key":value pair after a comma; a non-finite value as null. False when it doesn't fit.
bool put(char* out, size_t n, size_t* len, const char* key, float v, int decimals) {
    const int w = std::isfinite(v) ? snprintf(out + *len, n - *len, ",\"%s\":%.*f", key, decimals, (double)v)
                                   : snprintf(out + *len, n - *len, ",\"%s\":null", key);
    if (w < 0 || (size_t)w >= n - *len) return false;
    *len += (size_t)w;
    return true;
}

float realOr(const char* json, const char* key) {
    double d;
    return jsonflat::real(json, key, &d) ? (float)d : NAN;
}

bool closed(const char* json) {   // a whole object: '{' first and '}' last (spaces aside)
    while (*json == ' ' || *json == '\n' || *json == '\r' || *json == '\t') ++json;
    if (*json != '{') return false;
    const char* e = json + strlen(json);
    while (e > json && (e[-1] == ' ' || e[-1] == '\n' || e[-1] == '\r' || e[-1] == '\t')) --e;
    return e > json + 1 && e[-1] == '}';
}

float freshOr(Pid p, const float* value, const bool* have, const uint32_t* atMs, uint32_t nowMs) {
    const int i = (int)p;
    return have[i] && nowMs - atMs[i] < obdplan::staleAfterMs(p) ? value[i] : NAN;
}
}  // namespace

size_t encode(const Live& l, char* out, size_t n) {
    if (!out || n == 0) return 0;
    out[0] = '\0';
    int w = snprintf(out, n, "{\"v\":1,\"link\":%d", (int)l.link);
    if (w < 0 || (size_t)w >= n) {
        out[0] = '\0';
        return 0;
    }
    size_t len = (size_t)w;
    const bool ok = put(out, n, &len, "rpm", l.rpm, 0) && put(out, n, &len, "kmh", l.kmh, 0) &&
                    put(out, n, &len, "cool", l.coolC, 0) && put(out, n, &len, "iat", l.intakeC, 0) &&
                    put(out, n, &len, "thr", l.throttle, 1) && put(out, n, &len, "load", l.load, 1) &&
                    put(out, n, &len, "volt", l.volts, 1) &&
                    put(out, n, &len, "kml", l.kml, 1) && put(out, n, &len, "tkm", l.tripKm, 1) &&
                    put(out, n, &len, "tkml", l.tripKmL, 1) && put(out, n, &len, "rng", l.rangeKm, 0) &&
                    put(out, n, &len, "rsl", l.priceL, 2) && put(out, n, &len, "trs", l.tripRs, 2);
    if (ok) {
        w = snprintf(out + len, n - len, ",\"ts\":%lu,\"seq\":%lu}", (unsigned long)l.tripS, (unsigned long)l.seq);
        if (w >= 0 && (size_t)w < n - len) return len + (size_t)w;
    }
    out[0] = '\0';
    return 0;
}

bool decode(const char* json, Live* l) {
    if (!json || !l || !closed(json)) return false;
    long v = 0, link = 0, seq = 0;
    if (!jsonflat::num(json, "v", &v) || v != 1) return false;
    if (!jsonflat::num(json, "link", &link) || link < 0 || link > (long)obdplan::Link::Live) return false;
    Live out;
    out.link = (obdplan::Link)link;
    out.rpm = realOr(json, "rpm");
    out.kmh = realOr(json, "kmh");
    out.coolC = realOr(json, "cool");
    out.intakeC = realOr(json, "iat");
    out.throttle = realOr(json, "thr");
    out.load = realOr(json, "load");
    out.volts = realOr(json, "volt");
    out.kml = realOr(json, "kml");
    out.tripKm = realOr(json, "tkm");
    out.tripKmL = realOr(json, "tkml");
    out.rangeKm = realOr(json, "rng");
    out.priceL = realOr(json, "rsl");
    out.tripRs = realOr(json, "trs");
    long ts = 0;
    if (jsonflat::num(json, "ts", &ts) && ts >= 0) out.tripS = (uint32_t)ts;
    if (jsonflat::num(json, "seq", &seq) && seq >= 0) out.seq = (uint32_t)seq;
    *l = out;
    return true;
}

uint32_t Seq::update(const bool* have, const uint32_t* atMs) {
    uint32_t newest = 0;
    bool any = false;
    for (int i = 0; i < (int)Pid::COUNT; ++i)   // the latest across a millis() wrap: signed difference
        if (have[i] && (!any || (int32_t)(atMs[i] - newest) > 0)) {
            newest = atMs[i];
            any = true;
        }
    if (any && (!any_ || newest != newest_)) {
        any_ = true;
        newest_ = newest;
        ++seq_;
    }
    return seq_;
}

Pid slot(Pid p) {
    if (p == Pid::CoolantX) return Pid::Coolant;
    if (p == Pid::IntakeX) return Pid::Intake;
    return p;
}

Live fromReadings(obdplan::Link link, const float* value, const bool* have, const uint32_t* atMs, uint32_t nowMs) {
    Live l;
    l.link = link;
    l.rpm = freshOr(Pid::Rpm, value, have, atMs, nowMs);
    l.kmh = freshOr(Pid::Speed, value, have, atMs, nowMs);
    l.coolC = freshOr(Pid::Coolant, value, have, atMs, nowMs);
    l.intakeC = freshOr(Pid::Intake, value, have, atMs, nowMs);
    l.throttle = freshOr(Pid::Throttle, value, have, atMs, nowMs);
    l.load = freshOr(Pid::Load, value, have, atMs, nowMs);
    l.volts = freshOr(Pid::Battery, value, have, atMs, nowMs);
    return l;
}
}  // namespace obdwire
