#pragma once
#include <cstdint>
#include "camlist.h"
// Camera matching: one GPS fix -> what the deck shows and plays (off, no fix, alert, alarm) for the
// nearest point ahead on this road. Called about once a second, so the car moves up to ~28 m between
// fixes at 100 km/h: every threshold below holds with that step. A linear scan of the loaded points
// (at most camlist::MAX_POINTS, nothing to optimise). Equirectangular maths with cos(lat) of the fix:
// at 800 m its error is centimetres. No board dependencies, so it can move to the OBD deck later.
// PURE, host-tested.
namespace cameras {
// The thresholds, as the design fixed them (one place to change each).
constexpr uint16_t MIN_KMH = 10;              // under it: no alerts (course is noise when crawling)
constexpr uint32_t NOFIX_MS = 5000;           // no valid fix this long (moving or not) -> NoFix
constexpr uint32_t CANDIDATE_M = 800;         // points further away are not looked at
constexpr int COURSE_TOL_DEG = 40;            // car course vs the point's enforced heading
constexpr int BEARING_TOL_DEG = 30;           // bearing car->point vs the car's course (point ahead)
constexpr uint32_t CROSS_CHECK_M = 200;       // inside this, the sideways distance must be small...
constexpr int32_t CROSS_MAX_M = 30;           // ...under this (parallel service roads, overpasses)
constexpr uint32_t NEAR_M = 60;               // within this then moving away = passed
constexpr uint32_t PASSED_IGNORE_MS = 60000;  // a passed point is ignored this long
constexpr uint32_t ALERT_MIN_M = 400;         // speed / bump alert distance, at least...
constexpr uint32_t ALERT_AHEAD_S = 20;        // ...or this many seconds ahead, whichever is further
constexpr uint32_t REDLIGHT_ALERT_M = 250;    // red light: fixed alert distance, never an alarm
constexpr uint32_t ALARM_ZONE_M = 200;        // the last stretch where over the limit alarms

struct Fix {
    bool valid;
    int32_t latE6, lonE6;
    uint16_t kmh;     // already rounded down: the alarm compares it to the limit as-is (no tolerance)
    uint16_t course;  // 0..359
    uint32_t ms;      // the deck's millis() when the fix (or its absence) was seen
};
enum class Stage : uint8_t { Off, NoFix, Alert, Alarm };
struct Status {
    Stage stage = Stage::Off;
    int index = -1;  // into the loaded array; -1 with no active point
    uint16_t distM = 0;
    uint16_t kmh = 0;
};

class Matcher {
public:
    void load(const camlist::Point* pts, int n);  // keeps the pointer; clears passed state
    Status update(const Fix& f);                  // call per fix (about 1 Hz) and at least 1 Hz without one
private:
    const camlist::Point* pts_ = nullptr;
    int n_ = 0;
    uint32_t passedMs_[camlist::MAX_POINTS] = {};  // when it was passed; 0 = not passed
    bool near_[camlist::MAX_POINTS] = {};          // has been within NEAR_M on this approach
    uint32_t lastDist_[camlist::MAX_POINTS] = {};  // its distance at the last moving fix
    // Has alerted on this approach: stays eligible while it stays a candidate, so braking for the
    // alert (a shorter 20 s distance) doesn't take it back and chime again further on.
    bool alerted_[camlist::MAX_POINTS] = {};
    // The fix clock: the last valid fix's ms, or the first update's when none has come yet. Not
    // touched by load(): a new list doesn't make the GPS any fresher.
    bool clock_ = false;
    uint32_t lastFixMs_ = 0;
    Status last_;
};

// How far ahead a point of `kind` alerts at `kmh`: max(ALERT_MIN_M, ALERT_AHEAD_S x speed) for
// speed / bump, REDLIGHT_ALERT_M for a red light. The matcher's rule; the sign panel's countdown too.
uint32_t alertDistM(camlist::Kind kind, uint16_t kmh);

// Geometry helpers, exposed for tests: metres, degrees 0..359, signed metres (left < 0).
uint32_t distanceM(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2);
uint16_t bearing(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2);
int angleDiff(int a, int b);  // a - b, -179..180
int32_t crossTrackM(const Fix& f, const camlist::Point& p);  // the point's offset from the car's line of travel
}  // namespace cameras
