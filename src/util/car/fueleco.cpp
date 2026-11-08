#include "fueleco.h"
#include <cmath>

namespace fueleco {
namespace {
constexpr float AFR_GASOLINE = 14.7f, AFR_ETHANOL = 9.0f;
constexpr float RHO_GASOLINE = 0.745f, RHO_ETHANOL = 0.789f;   // kg/L
constexpr float R_AIR = 287.05f;                                // J/(kg K)

float ethanolMass(float pct) {           // mass fraction from the volume fraction
    const float e = pct / 100.0f;
    return RHO_ETHANOL * e / (RHO_ETHANOL * e + RHO_GASOLINE * (1.0f - e));
}
}

float airGs(float maf, float mapKpa, float rpm, float intakeC, const Car& c) {
    if (maf >= 0.0f) return maf;
    if (std::isnan(mapKpa) || std::isnan(rpm) || std::isnan(intakeC)) return NAN;
    const float kgS = mapKpa * 1000.0f * (c.displacementL / 1000.0f) * (rpm / 120.0f) * c.ve /
                      (R_AIR * (intakeC + 273.15f));
    return kgS * 1000.0f;
}

float afr(float ethanolPct) {
    const float w = ethanolMass(ethanolPct);         // kg of air per kg of fuel: linear by mass
    return w * AFR_ETHANOL + (1.0f - w) * AFR_GASOLINE;
}

float densityKgL(float ethanolPct) {
    const float e = ethanolPct / 100.0f;
    return RHO_ETHANOL * e + RHO_GASOLINE * (1.0f - e);
}

float litresPerHour(float airGs, float ethanolPct) {
    return airGs / afr(ethanolPct) / (densityKgL(ethanolPct) * 1000.0f) * 3600.0f;
}

bool fuelCut(float pedalPct, float throttlePct, float rpm, float speedKmh, float fuelSys, bool fuelSysFresh) {
    const bool released = !std::isnan(pedalPct) ? pedalPct < 2.0f : throttlePct < 2.0f;   // NAN: false
    if (!released || !(rpm > 1200.0f) || !(speedKmh > 20.0f)) return false;
    return !fuelSysFresh || std::isnan(fuelSys) || ((int)fuelSys & 4) != 0;
}

void Trip::step(uint32_t nowMs, float speedKmh, float air, bool fuelCut, float ethanolPct) {
    if (!started_) {
        started_ = true;
        lastMs_ = nowMs;
        return;
    }
    uint32_t dtMs = nowMs - lastMs_;                   // unsigned: right across the wrap
    lastMs_ = nowMs;
    if (dtMs > MAX_STEP_MS) dtMs = MAX_STEP_MS;
    const float dtH = dtMs / 3600000.0f;
    // Distance only while the fuel is known too (counted, or cut): km with no fuel counted, e.g.
    // before the first intake temperature, would push AVG and KM LEFT up.
    if (!std::isnan(speedKmh) && (fuelCut || !std::isnan(air))) km_ += speedKmh * dtH;
    if (fuelCut) {
        now_ = CAP;
        return;
    }
    if (std::isnan(air)) {
        now_ = NAN;
        return;
    }
    const float lph = litresPerHour(air, ethanolPct);
    litres_ += lph * dtH;
    if (std::isnan(speedKmh) || speedKmh < MIN_KMH || !(lph > 0.0f)) {
        now_ = NAN;
        return;
    }
    float inst = speedKmh / lph;
    if (inst > CAP) inst = CAP;
    if (std::isnan(now_) || now_ >= CAP) {
        now_ = inst;
    } else {
        const float a = (dtMs / 1000.0f) / (SMOOTH_S + dtMs / 1000.0f);
        now_ += a * (inst - now_);
    }
}

float Trip::avgKmL() const {
    if (!(litres_ > 0.0f)) return NAN;
    const float v = km_ / litres_;
    return v > CAP ? CAP : v;
}

float Trip::kmLeft(float fuelPct, const Car& c) const {
    const float avg = avgKmL();
    if (km_ < MIN_KM || std::isnan(fuelPct) || std::isnan(avg)) return NAN;
    return fuelPct / 100.0f * c.tankL * avg;
}
}
