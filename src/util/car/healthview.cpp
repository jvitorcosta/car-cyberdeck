#include "healthview.h"
#include <cstdio>
#include <cstring>

namespace healthview {
namespace {
using carhealth::Level;

Role roleOf(Level l) { return l == Level::Problem ? Role::Alert : l == Level::Note ? Role::Mid : Role::Text; }

void setLine(Line* l, Role r, const char* s) {
    size_t n = std::strlen(s);
    if (n > sizeof l->text - 1) n = sizeof l->text - 1;
    std::memcpy(l->text, s, n);
    l->text[n] = '\0';
    l->role = r;
}

// A number with `dec` decimals, clamped to `cap` so the text stays short; "--" when unknown or negative.
void num(char* out, size_t n, float v, int dec, float cap, char point = '.') {
    if (!(v >= 0.0f)) {  // NAN fails this too
        std::snprintf(out, n, "--");
        return;
    }
    if (v > cap) v = cap;
    std::snprintf(out, n, "%.*f", dec, static_cast<double>(v));
    if (point != '.')
        for (char* p = out; *p; ++p)
            if (*p == '.') *p = point;
}

void kmText(char* o, size_t n, float v) { num(o, n, v, v >= 100.0f ? 0 : 1, 9999.0f); }
void kmlText(char* o, size_t n, float v) { num(o, n, v, 1, 99.9f); }
void rangeText(char* o, size_t n, float v) { num(o, n, v, 0, 9999.0f); }
void rsText(char* o, size_t n, float v) {
    char t[16];
    num(t, sizeof t, v, 2, 999.99f, ',');
    std::snprintf(o, n, "R$ %s", t);
}
void voltText(char* o, size_t n, float v) {
    char t[12];
    num(t, sizeof t, v, 1, 99.9f);
    std::snprintf(o, n, "%sV", t);
}

// Nothing checked yet is "still checking", never "OK": a Note or a Problem is always shown.
bool unknown(const carhealth::Verdict& v) { return !v.known && v.level == Level::Ok; }

const char* battWord(Level l) { return l == Level::Ok ? "OK" : l == Level::Note ? "NOTE" : "LOW"; }
const char* engWord(Level l) { return l == Level::Ok ? "OK" : l == Level::Note ? "NOTE" : "CHECK"; }
}  // namespace

Check check(const carhealth::Verdict& batt, float volts, const carhealth::Verdict& eng, const tripsum::Summary* last,
            float lastCostRs, float rangeKm) {
    Check c{};
    char buf[96], a[16], b[16], d[24];

    voltText(a, sizeof a, volts);
    if (unknown(batt)) {
        setLine(&c.battery, Role::Dim, "BATTERY  CHECKING");
        setLine(&c.batteryWhy, Role::Dim, "");
    } else if (batt.level == Level::Ok) {
        std::snprintf(buf, sizeof buf, "BATTERY  %s  %s", a, battWord(batt.level));
        setLine(&c.battery, roleOf(batt.level), buf);
        // The deck only powers on with the engine running, so the detail is "is it charging", nothing more.
        if (volts >= 13.0f) std::snprintf(buf, sizeof buf, "CHARGING %s", a);
        else std::snprintf(buf, sizeof buf, "NO ISSUES SEEN");
        setLine(&c.batteryWhy, Role::Mid, buf);
    } else {
        std::snprintf(buf, sizeof buf, "BATTERY  %s  %s", a, battWord(batt.level));
        setLine(&c.battery, roleOf(batt.level), buf);
        setLine(&c.batteryWhy, roleOf(batt.level), batt.why);
    }

    if (unknown(eng)) {
        setLine(&c.engine, Role::Dim, "ENGINE  CHECKING");
        setLine(&c.engineWhy, Role::Dim, "");
    } else {
        std::snprintf(buf, sizeof buf, "ENGINE  %s", engWord(eng.level));
        setLine(&c.engine, roleOf(eng.level), buf);
        if (eng.level == Level::Ok) setLine(&c.engineWhy, Role::Mid, "NO FAULTS SEEN");
        else setLine(&c.engineWhy, roleOf(eng.level), eng.why);
    }

    if (last) {
        kmText(a, sizeof a, last->km);
        kmlText(b, sizeof b, last->kmL);
        rsText(d, sizeof d, lastCostRs);
        std::snprintf(buf, sizeof buf, "LAST TRIP %s KM  %s KM/L  %s", a, b, d);
    } else {
        std::snprintf(buf, sizeof buf, "LAST TRIP -- KM  -- KM/L  R$ --");
    }
    setLine(&c.lastTrip, Role::Mid, buf);

    rangeText(a, sizeof a, rangeKm);
    if (a[0] == '-') std::snprintf(buf, sizeof buf, "RANGE -- KM");
    else std::snprintf(buf, sizeof buf, "RANGE ~%s KM", a);
    setLine(&c.range, Role::Mid, buf);

    const int problems = (batt.level == Level::Problem ? 1 : 0) + (eng.level == Level::Problem ? 1 : 0);
    const int notes = (batt.level == Level::Note ? 1 : 0) + (eng.level == Level::Note ? 1 : 0);
    c.problem = problems > 0;
    if (problems > 0) {
        std::snprintf(buf, sizeof buf, ">%d ISSUE%s  [TAP TO CLOSE]", problems, problems == 1 ? "" : "S");
        setLine(&c.status, Role::Alert, buf);
    } else if (notes > 0) {
        std::snprintf(buf, sizeof buf, ">%d NOTE%s", notes, notes == 1 ? "" : "S");
        setLine(&c.status, Role::Mid, buf);
    } else if (unknown(batt) || unknown(eng)) {
        setLine(&c.status, Role::Dim, ">CHECKING...");  // all is well only once both were checked
    } else {
        setLine(&c.status, Role::Text, ">ALL SYSTEMS NOMINAL");
    }
    return c;
}

Band band(float tripKm, float kmlNow, float rangeKm, uint32_t seconds, float costRs, float volts, float coolantC) {
    Band b{};
    // Sized to what the helpers can write ("100.0" / "9999", "R$ 999,99", "99.9V", "999"), so the
    // target GCC can prove each field fits (-Wformat-truncation under -Werror).
    char a[6], c[10], v[6], cool[4];
    kmText(a, sizeof a, tripKm);
    std::snprintf(b.trip, sizeof b.trip, "%s KM", a);
    kmlText(a, sizeof a, kmlNow);
    std::snprintf(b.kmlNow, sizeof b.kmlNow, "%s KM/L", a);
    rangeText(a, sizeof a, rangeKm);
    std::snprintf(b.range, sizeof b.range, "%s KM LEFT", a);
    rsText(c, sizeof c, costRs);
    voltText(v, sizeof v, volts);
    num(cool, sizeof cool, coolantC, 0, 999.0f);
    const unsigned mins = static_cast<unsigned>(seconds / 60U);
    std::snprintf(b.small, sizeof b.small, "%u:%02u  %s  %s  %sC", mins / 60U, mins % 60U, c, v, cool);
    return b;
}

// A warning's kind: its why without the digits ("ENGINE HOT 108C" -> "ENGINE HOT C").
static void kindOf(const char* why, char* out, size_t n) {
    size_t k = 0;
    for (const char* p = why; *p && k + 1 < n; ++p)
        if (*p < '0' || *p > '9') out[k++] = *p;
    out[k] = 0;
}

bool Warnings::due(const char* kind, uint32_t nowMs) const {
    for (const Shown& s : shown_)
        if (s.used && std::strcmp(s.kind, kind) == 0) return nowMs - s.atMs >= WARN_EVERY_MS;
    return true;
}

void Warnings::mark(const char* kind, uint32_t nowMs) {
    // its own slot, else a free one, else the oldest
    Shown* slot = nullptr;
    for (Shown& s : shown_)
        if (s.used && std::strcmp(s.kind, kind) == 0) { slot = &s; break; }
    if (!slot)
        for (Shown& s : shown_)
            if (!s.used) { slot = &s; break; }
    if (!slot) {
        slot = &shown_[0];
        for (Shown& s : shown_)
            if (nowMs - s.atMs > nowMs - slot->atMs) slot = &s;
    }
    std::snprintf(slot->kind, sizeof slot->kind, "%s", kind);
    slot->atMs = nowMs;
    slot->used = true;
}

bool Warnings::next(const carhealth::Verdict& batt, const carhealth::Verdict& eng, bool critical, float kmh,
                    uint32_t nowMs, char* out, size_t n) {
    const bool stopped = kmh < 1.0f;  // NAN: false
    const carhealth::Verdict* order[] = {&eng, &batt};
    for (const carhealth::Verdict* v : order) {
        if (v->level != Level::Problem || !v->why[0]) continue;
        if (!stopped && !(critical && v == &eng)) continue;
        char kind[sizeof v->why];
        kindOf(v->why, kind, sizeof kind);
        if (!due(kind, nowMs)) continue;
        mark(kind, nowMs);
        if (n) std::snprintf(out, n, ">%s", v->why);
        return true;
    }
    return false;
}

Parked parked(const tripsum::Summary& s, float costRs, float rangeKm, const carhealth::Verdict& batt,
              const carhealth::Verdict& eng) {
    Parked p{};
    char buf[96], a[16], b[16], c[16];
    kmText(a, sizeof a, s.km);
    std::snprintf(buf, sizeof buf, "%s KM", a);
    setLine(&p.km, Role::Text, buf);
    std::snprintf(buf, sizeof buf, "%u MIN", static_cast<unsigned>((s.seconds / 60U) + ((s.seconds % 60U) >= 30U ? 1U : 0U)));
    setLine(&p.minutes, Role::Text, buf);
    kmlText(a, sizeof a, s.kmL);
    std::snprintf(buf, sizeof buf, "%s KM/L", a);
    setLine(&p.kml, Role::Text, buf);
    rsText(a, sizeof a, costRs);
    setLine(&p.cost, Role::Text, a);

    // Capped so the worst case "AVG 199 KM/H  IDLE 999 MIN  999 KM LEFT" is exactly 39 chars.
    num(a, sizeof a, s.avgKmh, 0, 199.0f);
    num(b, sizeof b, static_cast<float>((s.idleS / 60U) + ((s.idleS % 60U) >= 30U ? 1U : 0U)), 0, 999.0f);
    num(c, sizeof c, rangeKm, 0, 999.0f);
    std::snprintf(buf, sizeof buf, "AVG %s KM/H  IDLE %s MIN  %s KM LEFT", a, b, c);
    setLine(&p.small, Role::Mid, buf);

    if (unknown(batt)) {
        setLine(&p.battery, Role::Dim, "BATTERY --");
    } else {
        std::snprintf(buf, sizeof buf, "BATTERY %s", battWord(batt.level));
        setLine(&p.battery, roleOf(batt.level), buf);
    }
    if (unknown(eng)) {
        setLine(&p.engine, Role::Dim, "ENGINE --");
    } else {
        std::snprintf(buf, sizeof buf, "ENGINE %s", engWord(eng.level));
        setLine(&p.engine, roleOf(eng.level), buf);
    }
    setLine(&p.footer, Role::Text, ">SAFE TO SHUT DOWN");
    return p;
}

bool ParkGate::update(float kmh, float tripKm, uint32_t nowMs) {
    if (kmh > 3.0f) {  // moving: hide, and a dismiss is forgotten
        shown_ = false;
        dismissed_ = false;
        stopped_ = false;
        return false;
    }
    if (std::isnan(kmh) || kmh >= 1.0f) {  // unknown, or creeping: not stopped
        stopped_ = false;
        return shown_;
    }
    if (!stopped_) {
        stopped_ = true;
        stoppedSinceMs_ = nowMs;
    }
    if (!dismissed_ && tripKm >= 0.5f && static_cast<uint32_t>(nowMs - stoppedSinceMs_) >= 5000U) shown_ = true;
    return shown_;
}

void ParkGate::dismiss() {
    shown_ = false;
    dismissed_ = true;
}
}  // namespace healthview
