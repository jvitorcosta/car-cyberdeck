// The OBD deck's bench build (env obddeck_bench): obdlink's interface fed by util/car/benchdrive in
// place of the adapter, so everything after the link (health, trip, fuel book, /car) runs as in the
// car. No BLE and no tasks: snapshot() steps the drive, and only the loop task calls it here.
#include <Arduino.h>
#include <cmath>
#include <cstring>
#include "obdlink.h"
#include "../util/car/benchdrive.h"
#include "../util/car/fueleco.h"

namespace obdlink {
namespace {
using obd::Pid;
constexpr float ETHANOL_PCT = 30.0f;
View s_view{};
fueleco::Trip s_trip;
bool s_lastMil = false, s_codesSeen = false;

void put(Pid p, float v, uint32_t now) {
    if (std::isnan(v)) return;
    s_view.value[(int)p] = v;
    s_view.have[(int)p] = true;
    s_view.atMs[(int)p] = now;
}
}  // namespace

void start() {
    s_view.stored = s_view.pending = -1;
    s_view.kmlNow = NAN;
    Serial.println("[obd] BENCH BUILD: a simulated drive, no adapter (util/car/benchdrive)");
}

void snapshot(View& out) {
    const uint32_t now = millis();
    const benchdrive::Sample s = benchdrive::at(now);
    s_view.link = s.link;
    put(Pid::Rpm, s.rpm, now);
    put(Pid::Speed, s.kmh, now);
    put(Pid::Maf, s.mafGs, now);
    put(Pid::Battery, s.volts, now);
    put(Pid::Coolant, s.coolantC, now);
    put(Pid::TrimS, s.trimS, now);
    put(Pid::TrimL, s.trimL, now);
    if (s.link == obdplan::Link::Live) {
        s_trip.step(now, s.kmh, s.mafGs, false, ETHANOL_PCT);
        s_view.litresTotal = s_trip.litres();
        s_view.kmlNow = s_trip.nowKmL();
        s_view.airSteps++;
        if (!s_codesSeen || s.mil != s_lastMil) {   // as a good 03 / 07 read would say
            s_codesSeen = true;
            s_lastMil = s.mil;
            s_view.stored = s.stored;
            s_view.pending = 0;
            s_view.mil = s.mil;
            snprintf(s_view.codes[0], sizeof s_view.codes[0], "%s", s.mil ? "P0301" : "");
            s_view.codesReads++;
        }
    }
    out = s_view;
    out.nowMs = now;
}

float fresh(const View& v, Pid p, uint32_t nowMs) {
    return v.have[(int)p] && nowMs - v.atMs[(int)p] < obdplan::staleAfterMs(p) ? v.value[(int)p] : NAN;
}

unsigned linkStackLeft() { return 0; }
unsigned askStackLeft() { return 0; }
}  // namespace obdlink
