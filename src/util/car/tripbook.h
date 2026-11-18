#pragma once
#include <cstdint>
#include "fuelbook.h"
// The black deck's trip and fuel book, kept from the OBD link's running totals (fueleco::Trip over the
// whole power-on, which ends the trip): the seconds the link was live, and the fuel book charged as it
// goes, because a power cut gives no "trip end" to charge it at. Every CHARGE_MS the litres burnt since
// the last charge go to fuelbook::burn; the km/L it learns (fuelbook::learn) waits for LEARN_MIN_KM of
// driving, so a crawl out of the garage can't set a wild first value. At most CHARGE_MS of fuel is lost
// to a power cut: a small under-count. PURE, host-tested.
// Spec: docs/superpowers/specs/2026-11-18-obd2-hud-design.md ("Trip data")
namespace tripbook {
// fueleco since power-on: km and litres counted, and how many steps had an air reading (unchanged =
// nothing counted, so nothing is charged).
struct Totals {
    float km, litres;
    uint32_t airSteps;
};

class Meter {
public:
    // One call per loop pass (any rate). live: the link reads the car (obdplan::Link::Live). True when
    // *book changed, so the caller saves it. A gap longer than MAX_GAP_MS (a stalled loop) counts as
    // MAX_GAP_MS of live time.
    bool tick(uint32_t nowMs, bool live, const Totals& t, fuelbook::Book* book);
    // Charges now what tick would charge at its next 30 s: before a full tank is noted, so the fuel
    // burnt until then goes to the old tank, not the new one. True when *book changed. The km/l
    // learning carries on across it (it's the car's, not the tank's). Fuel burnt standing still is
    // charged and learnt with the next km: the average includes the idling, like the trip's.
    bool charge(uint32_t nowMs, const Totals& t, fuelbook::Book* book);
    uint32_t seconds() const { return liveMs_ / 1000; }   // the trip time: the engine on, this power-on
    static constexpr uint32_t CHARGE_MS = 30000, MAX_GAP_MS = 3000;
    static constexpr float LEARN_MIN_KM = 1.0f;
private:
    bool started_ = false;
    uint32_t lastMs_ = 0, chargedAt_ = 0, liveMs_ = 0;
    uint32_t steps_ = 0;               // airSteps at the last charge
    float litres_ = 0.0f;              // litres at the last charge
    float learnKm_ = 0.0f, learnL_ = 0.0f;   // charged but not learnt yet
    float km_ = 0.0f;                  // km at the last charge
};

// The phone page's price field, R$ per litre: "6.49", "6,49" or "6", up to two decimals, spaces around
// it skipped, inside fuelbook::PRICE_MIN..PRICE_MAX. False (and *price untouched) for anything else.
bool parsePrice(const char* s, float* price);
}  // namespace tripbook
