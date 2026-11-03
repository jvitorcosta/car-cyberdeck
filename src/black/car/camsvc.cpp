#include "camsvc.h"
#include <Arduino.h>
#include <Preferences.h>
#include <freertos/semphr.h>
#include <new>
#include "gps.h"
#include "../core/mem.h"

namespace camsvc {

// Two point arrays in PSRAM (mem::bigAlloc; ~5 KB each): the matcher reads the live one while a push
// parses into the spare, and they swap only once the new list is parsed and stored. camlist::parse
// writes rows before it knows the whole CSV is good, so parsing into the live one could leave it
// half replaced.
static camlist::Point* s_live = nullptr;
static camlist::Point* s_spare = nullptr;
static int s_count = 0;

// The GPS task updates the matcher; net/remote's task reads the status and swaps the list. The
// mutex covers all of these (a linear scan of at most 96 points: far too long for a critical section).
static SemaphoreHandle_t s_lock = nullptr;
static cameras::Matcher* s_matcher = nullptr; // ~1 KB of per-point state: in PSRAM with the lists
static cameras::Status s_status;
static cameras::Fix s_lastFix{};              // the last valid fix, for mark()
static volatile bool s_mute = false;
static bool s_broken = false;                 // begin() found no memory: snapshot() says NoFix
static void (*volatile s_cb)(cameras::Stage, cameras::Stage, camlist::Kind) = nullptr;
// Held from a status change until its callback has run (onFix on the GPS task, setList on net/remote's):
// the buzzer then hears the changes in the order they happened. Without it a list swap's "-> off"
// could reach it after the GPS task's next "-> alarm", and the beeps would stop with the alarm on
// (or never stop with it off).
static SemaphoreHandle_t s_order = nullptr;

static const char* NS = "cam";

struct Lock {
    Lock() { xSemaphoreTake(s_lock, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(s_lock); }
};
struct OrderLock {
    OrderLock() { xSemaphoreTake(s_order, portMAX_DELAY); }
    ~OrderLock() { xSemaphoreGive(s_order); }
};

static const char* stageName(cameras::Stage s) {
    switch (s) {
    case cameras::Stage::Off:   return "off";
    case cameras::Stage::NoFix: return "nofix";
    case cameras::Stage::Alert: return "alert";
    case cameras::Stage::Alarm: return "alarm";
    }
    return "?";
}

// The GPS task's sink (car/gps): one fix, or a once-a-second "none".
static void onFix(const cameras::Fix& f) {
    OrderLock order;
    cameras::Status prev, now;
    camlist::Kind kind = camlist::Kind::Speed;
    char id[sizeof camlist::Point::id] = "";
    void (*cb)(cameras::Stage, cameras::Stage, camlist::Kind);
    {
        Lock l;
        prev = s_status;
        now = s_matcher->update(f);
        if (f.valid) s_lastFix = f;
        // The matcher fills kmh only for an active point; /cam shows the speed in every stage while
        // the fix is fresh (camwire).
        now.kmh = s_lastFix.valid && f.ms - s_lastFix.ms <= cameras::NOFIX_MS ? s_lastFix.kmh : 0;
        s_status = now;
        // The point being entered, or the one just left: the buzzer wants to know which kind it was.
        const int i = now.index >= 0 ? now.index : prev.index;
        if (i >= 0 && i < s_count) {
            kind = s_live[i].kind;
            memcpy(id, s_live[i].id, sizeof id);
        }
        cb = s_cb;
    }
    if (now.stage == prev.stage) return;
    Serial.printf("[cam] %s -> %s %s %um %ukm/h\n", stageName(prev.stage), stageName(now.stage), id,
                  (unsigned)now.distM, (unsigned)now.kmh);
    if (cb) cb(prev.stage, now.stage, kind);   // outside the lock: it may take its time
}

void begin() {
    if (s_lock) return;
    s_lock = xSemaphoreCreateMutex();
    s_order = xSemaphoreCreateMutex();
    s_live = (camlist::Point*)mem::bigAlloc(sizeof(camlist::Point) * camlist::MAX_POINTS);
    s_spare = (camlist::Point*)mem::bigAlloc(sizeof(camlist::Point) * camlist::MAX_POINTS);
    void* m = mem::bigAlloc(sizeof(cameras::Matcher));
    if (!s_lock || !s_order || !s_live || !s_spare || !m) {
        // No matcher, no GPS task: GPS -- on both decks rather than an Off that reads as a clear road.
        s_broken = true;
        Serial.println("[cam] not watching: no memory (GPS --)");
        return;
    }
    int n = 0;
    Preferences p;
    if (p.begin(NS, true)) {   // read-only: false only when nothing was ever pushed
        const size_t len = p.getBytesLength("list");
        char* csv = len && len <= camlist::MAX_CSV ? (char*)mem::bigAlloc(len) : nullptr;
        if (csv && p.getBytes("list", csv, len) == len) {
            int bad = 0;
            n = camlist::parse(csv, len, s_live, camlist::MAX_POINTS, &bad);
            // Stored only after a good parse, so this means a changed parser: say so, run without.
            if (n < 0) { Serial.printf("[cam] stored list refused at line %d\n", bad); n = 0; }
        }
        free(csv);
        p.end();
    }
    s_count = n;
    s_matcher = new (m) cameras::Matcher();   // set last: the other calls test it for "running"
    s_matcher->load(s_live, n);
    Serial.printf("[cam] %d points\n", n);
    gps::start(onFix);
}

void snapshot(cameras::Status* s, camlist::Point* active, bool* mute) {
    cameras::Status st;
    camlist::Point pt{};
    if (s_broken) {
        st.stage = cameras::Stage::NoFix;
    } else if (s_lock) {
        Lock l;
        st = s_status;
        if (st.index >= 0 && st.index < s_count) pt = s_live[st.index];
    }
    if (s) *s = st;
    if (active) *active = pt;
    if (mute) *mute = s_mute;
}

bool setList(const char* csv, size_t len, int* badLine) {
    if (badLine) *badLine = 0;
    if (!s_matcher || len > camlist::MAX_CSV) return false;
    int bad = 0;
    const int n = camlist::parse(csv, len, s_spare, camlist::MAX_POINTS, &bad);
    if (n < 0) {
        if (badLine) *badLine = bad;
        return false;
    }
    // NVS before RAM: a list that can't be kept would be gone at the next start, so it's refused
    // now (and NVS keeps the old value when a write fails).
    Preferences p;
    bool stored = p.begin(NS, false) && p.putBytes("list", csv, len) == len;
    p.end();
    if (!stored) {
        Serial.printf("[cam] list of %u B not stored (NVS)\n", (unsigned)len);
        return false;
    }
    // The swap ends the stage the old list was in (its index points into it): the buzzer hears that
    // as a stage change like any other, so an alarm under way stops with it (the next fix starts
    // the new list's from Off).
    OrderLock order;
    cameras::Status prev;
    camlist::Kind kind = camlist::Kind::Speed;
    void (*cb)(cameras::Stage, cameras::Stage, camlist::Kind);
    {
        Lock l;
        prev = s_status;
        if (prev.index >= 0 && prev.index < s_count) kind = s_live[prev.index].kind;
        camlist::Point* old = s_live;
        s_live = s_spare;
        s_spare = old;
        s_count = n;
        s_matcher->load(s_live, n);
        s_status = cameras::Status{};
        cb = s_cb;
    }
    Serial.printf("[cam] new list: %d points\n", n);
    if (prev.stage != cameras::Stage::Off && cb) cb(prev.stage, cameras::Stage::Off, kind);
    return true;
}

void setMute(bool on) {
    s_mute = on;
    Serial.printf("[cam] mute %d\n", on ? 1 : 0);
}

struct Mark {
    uint32_t ms;
    int32_t latE6, lonE6;
    uint16_t course, kmh;
};
// In PSRAM, taken at the first use (512 B the deck needs only when marking): net/remote's task is
// the only caller.
static Mark* s_marks = nullptr;

// The stored marks into s_marks: their count, or -1 when they can't be read whole (no memory, a
// short or failed read). mark() then refuses rather than write over the ones it couldn't read.
static int loadMarks(Preferences& p) {
    if (!s_marks) s_marks = (Mark*)mem::bigAlloc(sizeof(Mark) * MAX_MARKS);
    if (!s_marks) return -1;
    size_t len = p.getBytesLength("marks");
    if (len > sizeof(Mark) * MAX_MARKS) len = sizeof(Mark) * MAX_MARKS;
    len -= len % sizeof(Mark);
    if (len && p.getBytes("marks", s_marks, len) != len) return -1;
    return (int)(len / sizeof(Mark));
}

MarkResult mark() {
    if (!s_matcher) return MarkResult::NoFix;   // never started: no GPS
    cameras::Fix f;
    {
        Lock l;
        f = s_lastFix;
    }
    if (!f.valid || millis() - f.ms > cameras::NOFIX_MS) return MarkResult::NoFix;
    Preferences p;
    if (!p.begin(NS, false)) {
        Serial.println("[cam] mark not stored (NVS)");
        return MarkResult::Storage;
    }
    int n = loadMarks(p);
    if (n < 0) {
        p.end();
        Serial.println("[cam] mark not stored: the marks can't be read back");
        return MarkResult::Storage;
    }
    if (n == MAX_MARKS) {   // full: the oldest goes (the PC tool should have collected it by now)
        memmove(s_marks, s_marks + 1, sizeof(Mark) * (MAX_MARKS - 1));
        n--;
    }
    s_marks[n++] = Mark{f.ms, f.latE6, f.lonE6, f.course, f.kmh};
    const size_t len = sizeof(Mark) * n;
    const bool ok = p.putBytes("marks", s_marks, len) == len;
    p.end();
    if (!ok) Serial.println("[cam] mark not stored (NVS)");
    else Serial.printf("[cam] mark %d saved\n", n);
    return ok ? MarkResult::Ok : MarkResult::Storage;
}

// Micro-degrees as "-2.965692": integer maths, the same digits the CSV uses.
static int fmtE6(char* out, size_t n, int32_t v) {
    const uint32_t a = v < 0 ? (uint32_t)(-(int64_t)v) : (uint32_t)v;
    return snprintf(out, n, "%s%lu.%06lu", v < 0 ? "-" : "", (unsigned long)(a / 1000000),
                    (unsigned long)(a % 1000000));
}

size_t marksCsv(char* out, size_t n) {
    if (!n) return 0;
    out[0] = '\0';
    Preferences p;
    if (!p.begin(NS, true)) return 0;   // nothing stored yet
    const int count = loadMarks(p);
    p.end();
    size_t used = 0;
    for (int i = 0; i < count; i++) {
        char lat[16], lon[16], row[64];
        fmtE6(lat, sizeof lat, s_marks[i].latE6);
        fmtE6(lon, sizeof lon, s_marks[i].lonE6);
        const int len = snprintf(row, sizeof row, "%lu,%s,%s,%u,%u\n", (unsigned long)s_marks[i].ms, lat,
                                 lon, (unsigned)s_marks[i].course, (unsigned)s_marks[i].kmh);
        if (len <= 0 || used + (size_t)len >= n) break;   // whole rows only
        memcpy(out + used, row, (size_t)len + 1);
        used += (size_t)len;
    }
    return used;
}

void clearMarks() {
    Preferences p;
    if (!p.begin(NS, false)) return;
    p.remove("marks");
    p.end();
    Serial.println("[cam] marks cleared");
}

void onStage(void (*cb)(cameras::Stage from, cameras::Stage to, camlist::Kind kind)) { s_cb = cb; }

}  // namespace camsvc
