#include "cameras.h"
#include <cmath>

namespace cameras {
namespace {
constexpr double PI = 3.14159265358979323846;
constexpr double M_PER_E6 = 6371000.0 * PI / 180.0 / 1e6;  // metres per micro-degree of latitude

// (east, north) metres from 1 to 2, scaled by cos(lat) of point 1 (the fix, in the matcher).
void offset(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2, double* east, double* north) {
    double cosLat = cos(lat1 / 1e6 * PI / 180.0);
    *east = (double)((int64_t)lon2 - lon1) * M_PER_E6 * cosLat;
    *north = (double)((int64_t)lat2 - lat1) * M_PER_E6;
}

int iabs(int v) { return v < 0 ? -v : v; }
}  // namespace

uint32_t distanceM(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2) {
    double e, n;
    offset(lat1, lon1, lat2, lon2, &e, &n);
    double d = sqrt(e * e + n * n);
    return d > 4.0e9 ? 4000000000u : (uint32_t)lround(d);
}

uint16_t bearing(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2) {
    double e, n;
    offset(lat1, lon1, lat2, lon2, &e, &n);
    long deg = lround(atan2(e, n) * 180.0 / PI);  // -180..180, 0 = north, clockwise
    return (uint16_t)((deg + 360) % 360);
}

int angleDiff(int a, int b) {
    int d = (a - b) % 360;
    if (d < 0) d += 360;  // 0..359
    return d > 180 ? d - 360 : d;
}

int32_t crossTrackM(const Fix& f, const camlist::Point& p) {
    double e, n;
    offset(f.latE6, f.lonE6, p.latE6, p.lonE6, &e, &n);
    double c = f.course * PI / 180.0;
    // The car->point vector against the course's right-hand normal (cos c, -sin c): right > 0.
    return (int32_t)lround(e * cos(c) - n * sin(c));
}

void Matcher::load(const camlist::Point* pts, int n) {
    pts_ = pts;
    n_ = n < 0 ? 0 : (n > camlist::MAX_POINTS ? camlist::MAX_POINTS : n);
    for (int i = 0; i < camlist::MAX_POINTS; i++) {
        passedMs_[i] = 0;
        near_[i] = false;
        lastDist_[i] = UINT32_MAX;
    }
    last_ = Status{};
}

Status Matcher::update(const Fix& f) {
    if (!f.valid) {
        // Lost while moving: keep showing what was there for a few seconds (a tunnel, a GPS hiccup),
        // then say so. Lost while stopped (or never had one): nothing to warn about.
        if (!hadFix_ || lastKmh_ < MIN_KMH) return last_ = Status{};
        if (f.ms - lastFixMs_ > NOFIX_MS) {
            last_ = Status{};
            last_.stage = Stage::NoFix;
        }
        return last_;
    }
    hadFix_ = true;
    lastFixMs_ = f.ms;
    lastKmh_ = f.kmh;
    // Crawling: the course is noise, so neither alerts nor passed state change.
    if (f.kmh < MIN_KMH) return last_ = Status{};

    Status best;
    uint32_t bestD = UINT32_MAX;
    for (int i = 0; i < n_; i++) {
        const camlist::Point& p = pts_[i];
        if (passedMs_[i] && f.ms - passedMs_[i] < PASSED_IGNORE_MS) continue;
        passedMs_[i] = 0;
        uint32_t d = distanceM(f.latE6, f.lonE6, p.latE6, p.lonE6);
        if (d > CANDIDATE_M || iabs(angleDiff(f.course, p.heading)) > COURSE_TOL_DEG) {
            near_[i] = false;  // off this approach (another road or the other way)
            lastDist_[i] = UINT32_MAX;
            continue;
        }
        // Passed: was within NEAR_M on this approach and is now further than at the last fix. At 1 Hz
        // and 100 km/h some fix lands within 14 m along the road, well inside NEAR_M.
        bool grows = d > lastDist_[i];
        lastDist_[i] = d;
        if (d <= NEAR_M) near_[i] = true;
        if (near_[i] && grows) {
            passedMs_[i] = f.ms ? f.ms : 1;  // 0 means "not passed"
            near_[i] = false;
            lastDist_[i] = UINT32_MAX;
            continue;
        }
        if (iabs(angleDiff(bearing(f.latE6, f.lonE6, p.latE6, p.lonE6), f.course)) > BEARING_TOL_DEG) continue;
        if (d < CROSS_CHECK_M) {
            int32_t x = crossTrackM(f, p);
            if (x <= -CROSS_MAX_M || x >= CROSS_MAX_M) continue;
        }
        bool red = p.kind == camlist::Kind::RedLight;
        uint32_t ahead = (uint32_t)f.kmh * ALERT_AHEAD_S * 1000 / 3600;
        uint32_t alertM = red ? REDLIGHT_ALERT_M : (ahead > ALERT_MIN_M ? ahead : ALERT_MIN_M);
        if (d > alertM || d >= bestD) continue;
        bestD = d;
        best.index = i;
        best.distM = (uint16_t)d;
        best.kmh = f.kmh;
        bool alarm = !red && p.limit != camlist::NO_LIMIT && d <= ALARM_ZONE_M && f.kmh > p.limit;
        best.stage = alarm ? Stage::Alarm : Stage::Alert;
    }
    return last_ = best;
}
}  // namespace cameras
