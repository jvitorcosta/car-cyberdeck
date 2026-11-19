#pragma once
#include <cstddef>
#include <cstdint>
#include "obdwire.h"
// The GAUGES Trip ticker (screen 3 of the CarDeck mocks): the text of its header and six cells, in the
// yellow deck's green VT323 terminal look. Economy first: km/l now, the trip's km/l, km, time, the
// range and what the trip cost. Unknown is "--": km/l now without the live link, the range and R$
// until a full tank with a price is noted (car/gauges' fuel book). Values are rounded so the digits
// stay still, and fit a cell at 36 px (at most 5 characters). PURE, host-tested.
// Spec: docs/superpowers/specs/2026-11-18-obd2-hud-design.md ("Trip ticker")
namespace tripticker {
enum class Cell : uint8_t { KmlNow, TripKml, TripKm, Range, TripTime, TripRs, COUNT };
// Over this km/l now the engine burns next to nothing: coasting (the link can't see a fuel cut, so
// fueleco's 99.9 cap or a value near it would show). A steady 1.5 L on the flat stays well under it.
constexpr float COAST_KML = 50.0f;
constexpr float LOW_RANGE_KM = 60.0f;   // the range turns red under this (only once there is one)
constexpr size_t TEXT = 8;              // a cell's text buffer

const char* label(Cell c);   // "KM/L NOW", "TRIP KM/L", "TRIP KM", "RANGE KM", "TRIP TIME", "TRIP R$"
// The cell's value: "12.4", "COAST", "1:05", "--". NUL-terminated, at most TEXT - 1 characters.
void text(Cell c, const obdwire::Live& l, char out[TEXT]);
// Red, inverted: the range under LOW_RANGE_KM.
bool alert(Cell c, const obdwire::Live& l);
// "> TRIP · 07 MIN" (the trip's whole minutes, in Latin-1: '\xB7' is the dot, as the VT323 tables
// have it), and the link's word for the top right: "OK" while live, else gaugehud::linkTag.
void header(const obdwire::Live& l, char* out, size_t n);
const char* status(obdplan::Link link);
}  // namespace tripticker
