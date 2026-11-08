#pragma once
#include <cstddef>
#include <cstdint>
#include "../util/car/carwire.h"
// The yellow deck's client of the OBD deck: step(), called by ledpanel's poll task between the
// lyrics and the camera band (one request at a time, so the deck holds one stack and one socket for
// all three), GETs the OBD deck's /car (signed, util/car/carwire) every second while it answers, and backs off
// 1 -> 2 -> 5 -> 10 s while it doesn't (no OBD deck in the car: one GET every 10 s, nothing else
// changes). The OBD deck is at its fixed hotspot address (172.20.10.12) on the iPhone hotspot, else
// looked up by its mDNS name (cardeck-obd) and kept through blips (util/deckaddr). The log tells when
// it is found, lost and back, and each back-off step, not every poll.
namespace carlink {
// The last snapshot that came, and millis() its seq last changed (0: none yet). A snapshot whose seq
// hasn't changed for STALE_MS is no data (the OBD deck stopped answering, or answers the same).
struct Reading {
    carwire::Snapshot snap{};
    uint32_t seqAt = 0;
};
constexpr uint32_t STALE_MS = 3000;

// From the voice task once WiFi is up; step() does nothing before it. mdnsUp starts the deck's mDNS
// (shared with the lyrics) and says whether it runs.
void begin(bool (*mdnsUp)());
// One poll when it is due (a GET of 1.5 s, 3 s deadline, and an mDNS query of 1.5 s at most), else
// nothing. From the poll task only.
void step();
// The latest reading into *out; false when nothing new came since the last take.
bool take(Reading* out);
// A full tank, from the voice fill-up: a signed GET /fill (?p=6.49 when price isn't null) to the
// address the OBD deck's last good /car answer came from, ~2-3 s at most; its 200 body ("ok 6.49",
// "ok none", "refused 649") into reply. False when it didn't answer 200, or hasn't answered a /car
// this power-on (no address yet), or nothing could be signed. From the voice task (blocks it).
bool fill(const char* price, char* reply, size_t n);
}  // namespace carlink
