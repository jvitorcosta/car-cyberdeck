#include <unity.h>
#include <cstring>
#include "../../src/util/car/camseq.h"

void setUp() {}
void tearDown() {}

using cameras::Stage;
using camclip::Slot;
using camlist::Kind;
using camseq::Step;

constexpr uint16_t ALL = (1u << camclip::COUNT) - 1;   // every clip in the pack
constexpr int NONE = -1;                               // next(): nothing due

static camseq::Event ev(Stage from, Stage to, Kind kind = Kind::Speed, const char* id = "mao-1",
                        uint8_t limit = 50) {
    camseq::Event e;
    e.from = from;
    e.to = to;
    e.kind = kind;
    strncpy(e.id, id ? id : "", sizeof e.id - 1);
    e.limit = limit;
    return e;
}
static camseq::Event gps(Stage from, Stage to, uint16_t lastKmh, bool first = false) {
    camseq::Event e = ev(from, to, Kind::Speed, "", camlist::NO_LIMIT);
    e.lastKmh = lastKmh;
    e.firstFix = first;
    return e;
}

static Step st;
// The next step's clip slot (or 100 + the note count for tones), NONE when nothing is due.
static int next(camseq::Seq& q, Stage live, const char* id, uint32_t now, bool mute = false) {
    if (!q.next(live, id, mute, now, &st)) return NONE;
    return st.clip ? (int)st.slot : 100 + st.n;
}
static int S(Slot s) { return (int)s; }
constexpr int TRIPLE = 105;        // camtone::alarmRepeat: three beeps, two rests

// ---- alert calls ----

void test_speed_full_call() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Off, Stage::Alert), 1000);
    TEST_ASSERT_EQUAL_UINT32(0, q.waitMs(1000));
    TEST_ASSERT_EQUAL_INT(S(Slot::Pon), next(q, Stage::Alert, "mao-1", 1000));
    TEST_ASSERT_EQUAL_UINT8(camseq::ALERT, st.prio);
    TEST_ASSERT_TRUE(st.muteable);
    TEST_ASSERT_EQUAL_INT(S(Slot::Torishimari), next(q, Stage::Alert, "mao-1", 2000));
    TEST_ASSERT_EQUAL_INT(S(Slot::L50), next(q, Stage::Alert, "mao-1", 3200));
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alert, "mao-1", 5000));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, q.waitMs(5000));
}

void test_bump_and_redlight_full_calls() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Off, Stage::Alert, Kind::Bump, "b-1", 30), 1000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Pon), next(q, Stage::Alert, "b-1", 1000));
    TEST_ASSERT_EQUAL_INT(S(Slot::Gochui), next(q, Stage::Alert, "b-1", 1000));
    TEST_ASSERT_EQUAL_INT(S(Slot::L30), next(q, Stage::Alert, "b-1", 1000));
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alert, "b-1", 1000));

    camseq::Seq r;
    r.clips(ALL);
    r.onEvent(ev(Stage::Off, Stage::Alert, Kind::RedLight, "r-1", camlist::NO_LIMIT), 1000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Pon), next(r, Stage::Alert, "r-1", 1000));
    TEST_ASSERT_EQUAL_INT(S(Slot::Gochui), next(r, Stage::Alert, "r-1", 1000));
    TEST_ASSERT_EQUAL_INT(NONE, next(r, Stage::Alert, "r-1", 1000));
}

void test_limit_without_a_line() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Off, Stage::Alert, Kind::Speed, "mao-1", 70), 1000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Pon), next(q, Stage::Alert, "mao-1", 1000));
    TEST_ASSERT_EQUAL_INT(S(Slot::Torishimari), next(q, Stage::Alert, "mao-1", 1000));
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alert, "mao-1", 1000));
    camseq::Seq u;   // unknown limit: no line either
    u.clips(ALL);
    u.onEvent(ev(Stage::Off, Stage::Alert, Kind::Bump, "b-1", camlist::NO_LIMIT), 1000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Pon), next(u, Stage::Alert, "b-1", 1000));
    TEST_ASSERT_EQUAL_INT(S(Slot::Gochui), next(u, Stage::Alert, "b-1", 1000));
    TEST_ASSERT_EQUAL_INT(NONE, next(u, Stage::Alert, "b-1", 1000));
}

static void drain(camseq::Seq& q, Stage live, const char* id, uint32_t now) {
    while (next(q, live, id, now) != NONE) {}
}

void test_short_call_within_20s() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Off, Stage::Alert, Kind::Speed, "a", 50), 1000);
    drain(q, Stage::Alert, "a", 1000);
    q.onEvent(ev(Stage::Alert, Stage::Off, Kind::Speed, "a", 50), 10000);
    // The next camera 19 s later, same limit: the chime and the limit only.
    q.onEvent(ev(Stage::Off, Stage::Alert, Kind::Speed, "b", 50), 29000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Pon), next(q, Stage::Alert, "b", 29000));
    TEST_ASSERT_EQUAL_INT(S(Slot::L50), next(q, Stage::Alert, "b", 29000));
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alert, "b", 29000));
}

void test_short_call_with_limit_change() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Off, Stage::Alert, Kind::Speed, "a", 50), 1000);
    drain(q, Stage::Alert, "a", 1000);
    // Straight from one point to the next (no Off between): the first leaves as the second enters.
    q.onEvent(ev(Stage::Alert, Stage::Alert, Kind::Bump, "b", 60), 8000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Pon), next(q, Stage::Alert, "b", 8000));
    TEST_ASSERT_EQUAL_INT(S(Slot::SpdChange), next(q, Stage::Alert, "b", 8000));
    TEST_ASSERT_EQUAL_INT(S(Slot::L60), next(q, Stage::Alert, "b", 8000));
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alert, "b", 8000));
}

void test_change_line_needs_both_limits() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Off, Stage::Alert, Kind::Speed, "a", camlist::NO_LIMIT), 1000);
    drain(q, Stage::Alert, "a", 1000);
    q.onEvent(ev(Stage::Alert, Stage::Off, Kind::Speed, "a", camlist::NO_LIMIT), 3000);
    q.onEvent(ev(Stage::Off, Stage::Alert, Kind::Speed, "b", 40), 5000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Pon), next(q, Stage::Alert, "b", 5000));
    TEST_ASSERT_EQUAL_INT(S(Slot::L40), next(q, Stage::Alert, "b", 5000));
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alert, "b", 5000));
}

void test_full_call_after_20s() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Off, Stage::Alert, Kind::Speed, "a", 50), 1000);
    drain(q, Stage::Alert, "a", 1000);
    q.onEvent(ev(Stage::Alert, Stage::Off, Kind::Speed, "a", 50), 4000);
    q.onEvent(ev(Stage::Off, Stage::Alert, Kind::Speed, "b", 60), 4000 + camseq::SHORT_CALL_MS);
    TEST_ASSERT_EQUAL_INT(S(Slot::Pon), next(q, Stage::Alert, "b", 24000));
    TEST_ASSERT_EQUAL_INT(S(Slot::Torishimari), next(q, Stage::Alert, "b", 24000));
    TEST_ASSERT_EQUAL_INT(S(Slot::L60), next(q, Stage::Alert, "b", 24000));
}

void test_short_call_counts_from_leaving_an_alarm() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Off, Stage::Alert, Kind::Speed, "a", 50), 1000);
    q.onEvent(ev(Stage::Alert, Stage::Alarm, Kind::Speed, "a", 50), 2000);
    drain(q, Stage::Alarm, "a", 2000);
    q.onEvent(ev(Stage::Alarm, Stage::Off, Kind::Speed, "a", 50), 30000);
    q.onEvent(ev(Stage::Off, Stage::Alert, Kind::Speed, "b", 50), 35000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Pon), next(q, Stage::Alert, "b", 35000));
    TEST_ASSERT_EQUAL_INT(S(Slot::L50), next(q, Stage::Alert, "b", 35000));
}

void test_redlight_short_call_unchanged() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Off, Stage::Alert, Kind::Speed, "a", 50), 1000);
    drain(q, Stage::Alert, "a", 1000);
    q.onEvent(ev(Stage::Alert, Stage::Off, Kind::Speed, "a", 50), 3000);
    q.onEvent(ev(Stage::Off, Stage::Alert, Kind::RedLight, "r", camlist::NO_LIMIT), 5000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Pon), next(q, Stage::Alert, "r", 5000));
    TEST_ASSERT_EQUAL_INT(S(Slot::Gochui), next(q, Stage::Alert, "r", 5000));
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alert, "r", 5000));
}

// ---- stale lines ----

void test_alert_sequence_dropped_on_alarm() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Off, Stage::Alert), 1000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Pon), next(q, Stage::Alert, "mao-1", 1000));
    q.onEvent(ev(Stage::Alert, Stage::Alarm), 1500);
    TEST_ASSERT_TRUE(q.cuts(camseq::ALERT, 1500));          // the chime playing now is cut
    TEST_ASSERT_EQUAL_INT(S(Slot::Speedni), next(q, Stage::Alarm, "mao-1", 1500));
    TEST_ASSERT_EQUAL_UINT8(camseq::ALARM, st.prio);
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alarm, "mao-1", 1600));   // no Torishimari, no limit
}

void test_alert_sequence_dropped_on_off() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Off, Stage::Alert), 1000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Pon), next(q, Stage::Alert, "mao-1", 1000));
    q.onEvent(ev(Stage::Alert, Stage::Off), 1500);
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Off, "", 1600));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, q.waitMs(1600));
}

void test_stale_check_reads_the_live_stage() {
    // The change that ended it is still queued: the live stage alone drops the rest.
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Off, Stage::Alert), 1000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Pon), next(q, Stage::Alert, "mao-1", 1000));
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Off, "", 2000));
    camseq::Seq r;   // live Alert, but for another point
    r.clips(ALL);
    r.onEvent(ev(Stage::Off, Stage::Alert), 1000);
    TEST_ASSERT_EQUAL_INT(NONE, next(r, Stage::Alert, "mao-2", 1000));
}

// ---- the alarm ----

void test_alarm_voice_then_beeps_then_voice_after_6s() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Alert, Stage::Alarm), 1000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Speedni), next(q, Stage::Alarm, "mao-1", 1000));
    TEST_ASSERT_TRUE(st.muteable);                         // a voice line: mute stops it
    TEST_ASSERT_EQUAL_UINT32(1500, q.waitMs(1000));
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alarm, "mao-1", 2400));
    TEST_ASSERT_EQUAL_INT(TRIPLE, next(q, Stage::Alarm, "mao-1", 2500));
    TEST_ASSERT_EQUAL_UINT16(1600, st.notes[0].hz);
    TEST_ASSERT_FALSE(st.muteable);
    TEST_ASSERT_EQUAL_INT(TRIPLE, next(q, Stage::Alarm, "mao-1", 4000));
    TEST_ASSERT_EQUAL_INT(TRIPLE, next(q, Stage::Alarm, "mao-1", 5500));
    // 6 s since the voice: it takes this slot.
    TEST_ASSERT_EQUAL_INT(S(Slot::Speedni), next(q, Stage::Alarm, "mao-1", 7000));
    TEST_ASSERT_EQUAL_INT(TRIPLE, next(q, Stage::Alarm, "mao-1", 8500));
}

void test_alarm_late_slot_counts_from_now() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Alert, Stage::Alarm), 1000);
    next(q, Stage::Alarm, "mao-1", 1000);
    TEST_ASSERT_EQUAL_INT(TRIPLE, next(q, Stage::Alarm, "mao-1", 3100));   // the voice ran long
    TEST_ASSERT_EQUAL_UINT32(1500, q.waitMs(3100));
}

void test_alarm_silent_once_it_ends() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Alert, Stage::Alarm), 1000);
    next(q, Stage::Alarm, "mao-1", 1000);
    q.onEvent(ev(Stage::Alarm, Stage::Alert), 1800);       // braked: no chime either
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, q.waitMs(1800));
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alert, "mao-1", 2500));
    TEST_ASSERT_FALSE(q.cuts(camseq::NONE, 2500));
}

void test_alarm_tick_trusts_the_live_stage() {
    // A lost "-> off" (full queue): the next slot checks the live stage and stops.
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Alert, Stage::Alarm), 1000);
    next(q, Stage::Alarm, "mao-1", 1000);
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Off, "", 2500));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, q.waitMs(2500));
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alarm, "mao-1", 4000));   // it took a change to start
}

void test_alarm_flicker_keeps_the_cadence() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Alert, Stage::Alarm), 1000);
    next(q, Stage::Alarm, "mao-1", 1000);
    q.onEvent(ev(Stage::Alarm, Stage::Alert), 1400);
    // Back over the limit 600 ms after the voice: it waits for its 1.5 s slot, and that slot beeps
    // (the voice played under 6 s ago).
    q.onEvent(ev(Stage::Alert, Stage::Alarm), 1600);
    TEST_ASSERT_EQUAL_UINT32(900, q.waitMs(1600));
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alarm, "mao-1", 1600));
    TEST_ASSERT_EQUAL_INT(TRIPLE, next(q, Stage::Alarm, "mao-1", 2500));
}

void test_millis_wrap() {
    camseq::Seq q;
    const uint32_t t = 0xFFFFFF00u;
    q.onEvent(ev(Stage::Alert, Stage::Alarm), t);
    TEST_ASSERT_EQUAL_INT(TRIPLE, next(q, Stage::Alarm, "mao-1", t));   // no clips: the beep
    TEST_ASSERT_EQUAL_UINT32(1500, q.waitMs(t));
    TEST_ASSERT_EQUAL_INT(TRIPLE, next(q, Stage::Alarm, "mao-1", t + 1500));
}

// ---- GPS lines ----

void test_gps_lost_only_when_driving() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(gps(Stage::Off, Stage::NoFix, 9), 1000);       // parked, or crawling: no line
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::NoFix, "", 1000));
    q.onEvent(gps(Stage::NoFix, Stage::Off, 0), 2000);
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Off, "", 2000));   // never said lost: no "back"
    q.onEvent(gps(Stage::Off, Stage::NoFix, 10), 3000);
    TEST_ASSERT_EQUAL_INT(S(Slot::GpsNg), next(q, Stage::NoFix, "", 3000));
    TEST_ASSERT_EQUAL_UINT8(camseq::GPS, st.prio);
    q.onEvent(gps(Stage::NoFix, Stage::NoFix, 10), 4000);    // the same loss again: once
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::NoFix, "", 4000));
}

void test_gps_back_after_the_lost_line() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(gps(Stage::Alert, Stage::NoFix, 40), 1000);
    TEST_ASSERT_EQUAL_INT(S(Slot::GpsNg), next(q, Stage::NoFix, "", 1000));
    q.onEvent(gps(Stage::NoFix, Stage::Off, 40), 9000);
    TEST_ASSERT_EQUAL_INT(S(Slot::GpsOk), next(q, Stage::Off, "", 9000));
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Off, "", 9000));
    // A second loss is a new one.
    q.onEvent(gps(Stage::Off, Stage::NoFix, 40), 20000);
    TEST_ASSERT_EQUAL_INT(S(Slot::GpsNg), next(q, Stage::NoFix, "", 20000));
}

void test_gps_back_before_the_line_played() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(gps(Stage::Off, Stage::NoFix, 40), 1000);
    q.onEvent(gps(Stage::NoFix, Stage::Off, 40), 1200);      // back before it was said: neither
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Off, "", 1200));
}

void test_first_fix_once() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(gps(Stage::Off, Stage::Off, 0, true), 1000);
    TEST_ASSERT_EQUAL_INT(S(Slot::GpsFirst), next(q, Stage::Off, "", 1000));
    q.onEvent(gps(Stage::NoFix, Stage::Off, 0, true), 9000);
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Off, "", 9000));
}

void test_gps_lines_give_way_to_alerts() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(gps(Stage::Off, Stage::Off, 0, true), 1000);
    TEST_ASSERT_EQUAL_INT(S(Slot::GpsFirst), next(q, Stage::Off, "", 1000));
    TEST_ASSERT_FALSE(q.cuts(camseq::GPS, 1000));
    q.onEvent(ev(Stage::Off, Stage::Alert), 1100);
    TEST_ASSERT_TRUE(q.cuts(camseq::GPS, 1100));
    TEST_ASSERT_FALSE(q.cuts(camseq::ALERT, 1100));          // the same rank never cuts
    // Both waiting: the alert first.
    camseq::Seq r;
    r.clips(ALL);
    r.onEvent(gps(Stage::Off, Stage::Off, 0, true), 1000);
    r.onEvent(ev(Stage::Off, Stage::Alert), 1000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Pon), next(r, Stage::Alert, "mao-1", 1000));
    drain(r, Stage::Alert, "mao-1", 1000);   // ends with the GPS line
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, r.waitMs(1000));
}

// ---- mute ----

void test_mute_keeps_alarm_beeps_drops_voice() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Off, Stage::Alert), 1000);
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alert, "mao-1", 1000, true));
    q.onEvent(ev(Stage::Alert, Stage::Alarm), 2000);
    TEST_ASSERT_EQUAL_INT(TRIPLE, next(q, Stage::Alarm, "mao-1", 2000, true));   // in the voice's place
    TEST_ASSERT_EQUAL_INT(TRIPLE, next(q, Stage::Alarm, "mao-1", 3500, true));
    // Unmuted mid-alarm: the voice never played, so the next slot is the voice.
    TEST_ASSERT_EQUAL_INT(S(Slot::Speedni), next(q, Stage::Alarm, "mao-1", 5000));
    q.onEvent(ev(Stage::Alarm, Stage::Off), 6000);
    q.onEvent(gps(Stage::Off, Stage::NoFix, 50), 7000);
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::NoFix, "", 7000, true));
    q.onEvent(gps(Stage::NoFix, Stage::Off, 50), 8000);
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Off, "", 8000));      // the lost line never played
}

void test_mute_mid_sequence_drops_the_rest() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Off, Stage::Alert), 1000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Pon), next(q, Stage::Alert, "mao-1", 1000));
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alert, "mao-1", 1500, true));
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alert, "mao-1", 1600));
}

// ---- the chime guard (kept from camcue) ----

void test_same_point_flicker_within_3s_is_quiet() {
    camseq::Seq q;
    q.onEvent(ev(Stage::Off, Stage::Alert), 1000);
    TEST_ASSERT_EQUAL_INT(103, next(q, Stage::Alert, "mao-1", 1000));
    q.onEvent(ev(Stage::Alert, Stage::Off), 2000);
    // Back over 10 km/h a second later, same camera: no second chime.
    q.onEvent(ev(Stage::Off, Stage::Alert), 3000);
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alert, "mao-1", 3000));
    // ...and the guard runs from the last chime, not from the flicker.
    q.onEvent(ev(Stage::Alert, Stage::Off), 3500);
    q.onEvent(ev(Stage::Off, Stage::Alert), 1000 + camseq::CHIME_GUARD_MS);
    TEST_ASSERT_EQUAL_INT(103, next(q, Stage::Alert, "mao-1", 4000));
}

void test_another_point_chimes_at_once() {
    camseq::Seq q;
    q.onEvent(ev(Stage::Off, Stage::Alert, Kind::Speed, "mao-1"), 1000);
    drain(q, Stage::Alert, "mao-1", 1000);
    q.onEvent(ev(Stage::Alert, Stage::Off, Kind::Speed, "mao-1"), 1500);
    q.onEvent(ev(Stage::Off, Stage::Alert, Kind::Bump, "mao-2"), 2000);
    TEST_ASSERT_EQUAL_INT(103, next(q, Stage::Alert, "mao-2", 2000));
    TEST_ASSERT_EQUAL_UINT16(600, st.notes[0].hz);          // the bump's low pair
}

void test_null_id_counts_as_empty() {
    camseq::Seq q;
    q.onEvent(ev(Stage::Off, Stage::Alert, Kind::RedLight, nullptr, 0), 1000);
    TEST_ASSERT_EQUAL_INT(101, next(q, Stage::Alert, "", 1000));
    q.onEvent(ev(Stage::Alert, Stage::Off, Kind::RedLight, "", 0), 1500);
    q.onEvent(ev(Stage::Off, Stage::Alert, Kind::RedLight, "", 0), 2000);
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alert, nullptr, 2000));
}

// ---- fallback beeps ----

void test_no_pack_plays_the_old_beeps() {
    camseq::Seq q;   // no clips at all
    q.onEvent(ev(Stage::Off, Stage::Alert, Kind::Speed, "s"), 1000);
    TEST_ASSERT_EQUAL_INT(103, next(q, Stage::Alert, "s", 1000));
    TEST_ASSERT_EQUAL_UINT16(1000, st.notes[0].hz);
    TEST_ASSERT_TRUE(st.muteable);
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alert, "s", 1000));
    q.onEvent(ev(Stage::Alert, Stage::Off, Kind::Speed, "s"), 2000);
    q.onEvent(ev(Stage::Off, Stage::Alert, Kind::RedLight, "r", 0), 3000);
    TEST_ASSERT_EQUAL_INT(101, next(q, Stage::Alert, "r", 3000));
    TEST_ASSERT_EQUAL_UINT16(500, st.notes[0].ms);
    q.onEvent(ev(Stage::Alert, Stage::Alarm, Kind::Speed, "r2", 40), 3500);
    TEST_ASSERT_EQUAL_INT(TRIPLE, next(q, Stage::Alarm, "r2", 3500));
    q.onEvent(gps(Stage::Alarm, Stage::NoFix, 50), 4000);
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::NoFix, "", 4000));   // GPS lines have no beep
    q.onEvent(gps(Stage::NoFix, Stage::Off, 50, true), 5000);
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Off, "", 5000));
}

void test_missing_chime_becomes_the_beep() {
    camseq::Seq q;
    q.clips(ALL & ~(1u << (int)Slot::Pon) & ~(1u << (int)Slot::L50));
    q.onEvent(ev(Stage::Off, Stage::Alert, Kind::Speed, "s", 50), 1000);
    TEST_ASSERT_EQUAL_INT(103, next(q, Stage::Alert, "s", 1000));
    TEST_ASSERT_EQUAL_INT(S(Slot::Torishimari), next(q, Stage::Alert, "s", 1000));
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alert, "s", 1000));   // a missing limit line is skipped
}

void test_missing_alarm_voice_beeps() {
    camseq::Seq q;
    q.clips(ALL & ~(1u << (int)Slot::Speedni));
    q.onEvent(ev(Stage::Alert, Stage::Alarm), 1000);
    TEST_ASSERT_EQUAL_INT(TRIPLE, next(q, Stage::Alarm, "mao-1", 1000));
}

// ---- review fixes ----

void test_alarm_step_stops_when_the_alarm_ends() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Alert, Stage::Alarm), 1000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Speedni), next(q, Stage::Alarm, "mao-1", 1000));
    const Step voice = st;
    TEST_ASSERT_FALSE(q.stops(voice));
    q.onEvent(ev(Stage::Alarm, Stage::Alert), 1300);       // braked mid-line: silence now
    TEST_ASSERT_TRUE(q.stops(voice));

    camseq::Seq b;   // a beep, the stage going Off
    b.onEvent(ev(Stage::Alert, Stage::Alarm), 1000);
    TEST_ASSERT_EQUAL_INT(TRIPLE, next(b, Stage::Alarm, "mao-1", 1000));
    const Step beep = st;
    TEST_ASSERT_FALSE(b.stops(beep));
    b.onEvent(ev(Stage::Alarm, Stage::Off), 1100);
    TEST_ASSERT_TRUE(b.stops(beep));

    camseq::Seq l;   // the "-> off" lost: the live stage read between writes
    l.onEvent(ev(Stage::Alert, Stage::Alarm), 1000);
    TEST_ASSERT_EQUAL_INT(TRIPLE, next(l, Stage::Alarm, "mao-1", 1000));
    const Step lost = st;
    l.sync(Stage::Alarm);
    TEST_ASSERT_FALSE(l.stops(lost));
    l.sync(Stage::Off);
    TEST_ASSERT_TRUE(l.stops(lost));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, l.waitMs(1100));
}

void test_alert_steps_never_stop_for_the_alarm_rule() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Off, Stage::Alert), 1000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Pon), next(q, Stage::Alert, "mao-1", 1000));
    TEST_ASSERT_FALSE(q.stops(st));
}

void test_mute_alarm_line_muteable_beeps_not() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Alert, Stage::Alarm), 1000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Speedni), next(q, Stage::Alarm, "mao-1", 1000));
    TEST_ASSERT_TRUE(st.muteable);
    TEST_ASSERT_EQUAL_INT(TRIPLE, next(q, Stage::Alarm, "mao-1", 2500));
    TEST_ASSERT_FALSE(st.muteable);
}

void test_new_point_alarm_gets_its_line() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Alert, Stage::Alarm, Kind::Speed, "a", 50), 1000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Speedni), next(q, Stage::Alarm, "a", 1000));
    q.onEvent(ev(Stage::Alarm, Stage::Off, Kind::Speed, "a", 50), 2000);
    q.onEvent(ev(Stage::Off, Stage::Alarm, Kind::Speed, "b", 40), 3000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Speedni), next(q, Stage::Alarm, "b", 3000));
    // Straight from one alarm into another point's: its line at the next slot.
    q.onEvent(ev(Stage::Alarm, Stage::Alarm, Kind::Speed, "c", 30), 3500);
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alarm, "c", 3500));
    TEST_ASSERT_EQUAL_INT(S(Slot::Speedni), next(q, Stage::Alarm, "c", 4500));
}

void test_same_point_back_into_alarm_beeps() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Alert, Stage::Alarm, Kind::Speed, "a", 50), 1000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Speedni), next(q, Stage::Alarm, "a", 1000));
    q.onEvent(ev(Stage::Alarm, Stage::Off, Kind::Speed, "a", 50), 2000);   // under 10 km/h a moment
    q.onEvent(ev(Stage::Off, Stage::Alarm, Kind::Speed, "a", 50), 3000);
    TEST_ASSERT_EQUAL_INT(TRIPLE, next(q, Stage::Alarm, "a", 3000));
}

void test_gps_back_straight_into_alert() {
    // The fix returns with a camera already in range: the alert's call first (it outranks GPS),
    // then the "back" line.
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(gps(Stage::Off, Stage::NoFix, 40), 1000);
    TEST_ASSERT_EQUAL_INT(S(Slot::GpsNg), next(q, Stage::NoFix, "", 1000));
    q.onEvent(ev(Stage::NoFix, Stage::Alert), 6000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Pon), next(q, Stage::Alert, "mao-1", 6000));
    TEST_ASSERT_EQUAL_INT(S(Slot::Torishimari), next(q, Stage::Alert, "mao-1", 6000));
    TEST_ASSERT_EQUAL_INT(S(Slot::L50), next(q, Stage::Alert, "mao-1", 6000));
    TEST_ASSERT_EQUAL_INT(S(Slot::GpsOk), next(q, Stage::Alert, "mao-1", 6000));
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alert, "mao-1", 6000));
}

void test_alarm_to_another_points_alert_is_a_short_call() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Off, Stage::Alert, Kind::Speed, "a", 50), 1000);
    drain(q, Stage::Alert, "a", 1000);
    q.onEvent(ev(Stage::Alert, Stage::Alarm, Kind::Speed, "a", 50), 2000);
    drain(q, Stage::Alarm, "a", 2000);
    q.onEvent(ev(Stage::Alarm, Stage::Alert, Kind::Speed, "b", 60), 3000);
    TEST_ASSERT_EQUAL_INT(S(Slot::Pon), next(q, Stage::Alert, "b", 3000));
    TEST_ASSERT_EQUAL_INT(S(Slot::SpdChange), next(q, Stage::Alert, "b", 3000));
    TEST_ASSERT_EQUAL_INT(S(Slot::L60), next(q, Stage::Alert, "b", 3000));
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alert, "b", 3000));
}

// ---- marks ----

static camseq::Event mark(uint8_t m) {
    camseq::Event e;
    e.mark = m;
    return e;
}

void test_mark_answer_beeps_even_muted_and_leaves_the_stage() {
    camseq::Seq q;
    q.clips(ALL);
    q.onEvent(ev(Stage::Off, Stage::Alert), 1000);
    q.onEvent(mark(camseq::MARK_SAVED), 1100);
    TEST_ASSERT_EQUAL_INT(S(Slot::Pon), next(q, Stage::Alert, "mao-1", 1100));   // the alert first
    TEST_ASSERT_EQUAL_INT(S(Slot::Torishimari), next(q, Stage::Alert, "mao-1", 2000));
    TEST_ASSERT_EQUAL_INT(S(Slot::L50), next(q, Stage::Alert, "mao-1", 3000));
    TEST_ASSERT_EQUAL_INT(103, next(q, Stage::Alert, "mao-1", 4000));             // the chirp
    TEST_ASSERT_FALSE(st.muteable);
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alert, "mao-1", 4500));
    q.onEvent(mark(camseq::MARK_FAILED), 5000);
    TEST_ASSERT_EQUAL_UINT32(0, q.waitMs(5000));
    TEST_ASSERT_EQUAL_INT(101, next(q, Stage::Alert, "mao-1", 5000, true));      // muted: still heard
}

void test_alarm_slot_goes_before_a_mark() {
    camseq::Seq q;
    q.clips(0);
    q.onEvent(ev(Stage::Alert, Stage::Alarm), 1000);
    q.onEvent(mark(camseq::MARK_SAVED), 1000);
    TEST_ASSERT_EQUAL_INT(TRIPLE, next(q, Stage::Alarm, "mao-1", 1000));
    TEST_ASSERT_EQUAL_INT(NONE, next(q, Stage::Alarm, "mao-1", 1200));          // between slots: waits
    q.onEvent(ev(Stage::Alarm, Stage::Off), 1300);
    TEST_ASSERT_EQUAL_INT(103, next(q, Stage::Off, "", 1300));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_speed_full_call);
    RUN_TEST(test_bump_and_redlight_full_calls);
    RUN_TEST(test_limit_without_a_line);
    RUN_TEST(test_short_call_within_20s);
    RUN_TEST(test_short_call_with_limit_change);
    RUN_TEST(test_change_line_needs_both_limits);
    RUN_TEST(test_full_call_after_20s);
    RUN_TEST(test_short_call_counts_from_leaving_an_alarm);
    RUN_TEST(test_redlight_short_call_unchanged);
    RUN_TEST(test_alert_sequence_dropped_on_alarm);
    RUN_TEST(test_alert_sequence_dropped_on_off);
    RUN_TEST(test_stale_check_reads_the_live_stage);
    RUN_TEST(test_alarm_voice_then_beeps_then_voice_after_6s);
    RUN_TEST(test_alarm_late_slot_counts_from_now);
    RUN_TEST(test_alarm_silent_once_it_ends);
    RUN_TEST(test_alarm_tick_trusts_the_live_stage);
    RUN_TEST(test_alarm_flicker_keeps_the_cadence);
    RUN_TEST(test_millis_wrap);
    RUN_TEST(test_gps_lost_only_when_driving);
    RUN_TEST(test_gps_back_after_the_lost_line);
    RUN_TEST(test_gps_back_before_the_line_played);
    RUN_TEST(test_first_fix_once);
    RUN_TEST(test_gps_lines_give_way_to_alerts);
    RUN_TEST(test_mute_keeps_alarm_beeps_drops_voice);
    RUN_TEST(test_mute_mid_sequence_drops_the_rest);
    RUN_TEST(test_same_point_flicker_within_3s_is_quiet);
    RUN_TEST(test_another_point_chimes_at_once);
    RUN_TEST(test_null_id_counts_as_empty);
    RUN_TEST(test_no_pack_plays_the_old_beeps);
    RUN_TEST(test_missing_chime_becomes_the_beep);
    RUN_TEST(test_missing_alarm_voice_beeps);
    RUN_TEST(test_alarm_step_stops_when_the_alarm_ends);
    RUN_TEST(test_alert_steps_never_stop_for_the_alarm_rule);
    RUN_TEST(test_mute_alarm_line_muteable_beeps_not);
    RUN_TEST(test_new_point_alarm_gets_its_line);
    RUN_TEST(test_same_point_back_into_alarm_beeps);
    RUN_TEST(test_gps_back_straight_into_alert);
    RUN_TEST(test_alarm_to_another_points_alert_is_a_short_call);
    RUN_TEST(test_mark_answer_beeps_even_muted_and_leaves_the_stage);
    RUN_TEST(test_alarm_slot_goes_before_a_mark);
    return UNITY_END();
}
