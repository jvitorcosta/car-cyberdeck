#pragma once
#include <cstdint>
// The gauge cluster's fuel economy (spec 2026-10-19-gauge-cluster "Derived values"): fuel burnt
// from the engine's air, km/l now, the trip average and km left. Air comes from the MAF, or by
// speed-density from manifold pressure, RPM and intake temperature; the stoichiometric ratio and
// density follow the ethanol blend (Brazilian pump gasoline is E30). Fuel cut (coasting off the
// pedal) burns nothing. NAN means "unknown" in and out. PURE, host-tested.
namespace fueleco {
struct Car { float displacementL, ve, tankL, ethanolPct; };   // the City: 1.497, 0.85, 44, 30

// Air mass flow, g/s: maf when it's a reading (>= 0), else speed-density; NAN without either.
float airGs(float maf, float mapKpa, float rpm, float intakeC, const Car& c);
float afr(float ethanolPct);             // stoichiometric for the blend (by mass): 14.7 .. 9.0
float densityKgL(float ethanolPct);      // blend density: 0.745 .. 0.789
float litresPerHour(float airGs, float ethanolPct);
// Coasting fuel cut: the pedal released (else the throttle, when there's no pedal reading), over
// 1200 RPM and 20 km/h. A fresh fuel-system status (PID 03) only confirms it (bit "4" = open loop:
// decel fuel cut, but also full-load enrichment, so it can't decide alone); a stale one is ignored.
bool fuelCut(float pedalPct, float throttlePct, float rpm, float speedKmh, float fuelSys, bool fuelSysFresh);

class Trip {
public:
    // One step at nowMs: speed km/h, air g/s (NAN = unknown), fuelCut (no fuel burnt), the
    // blend's ethanol %. Steps longer than MAX_STEP_MS count as MAX_STEP_MS (a Bluetooth stall).
    void step(uint32_t nowMs, float speedKmh, float airGs, bool fuelCut, float ethanolPct);
    float nowKmL() const { return now_; }   // smoothed; NAN below MIN_KMH or without air; 99.9 cap
    float avgKmL() const;                   // NAN until some fuel has been counted
    float kmLeft(float fuelPct, const Car& c) const;   // NAN under MIN_KM or without a fuel level
    float km() const { return km_; }
    static constexpr uint32_t MAX_STEP_MS = 2000;
    static constexpr float MIN_KMH = 3.0f, MIN_KM = 2.0f, CAP = 99.9f, SMOOTH_S = 1.5f;
private:
    bool started_ = false;
    uint32_t lastMs_ = 0;
    float km_ = 0.0f, litres_ = 0.0f, now_ = __builtin_nanf("");
};
}
