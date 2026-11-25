#pragma once
#include <cstdint>
// The City's fuel level in litres, from the meter ECU (Honda Mode 22, DID 7029): the one read OBDb's
// Honda-City signalset lists for the tank ("CITY_FLV", 16 bits / 100, years 2015 to 2022). The car has no
// standard fuel-level PID (2F is unsupported, verified), so without this the range needs a FULL TANK
// click. UNVERIFIED on a 2022 car: the byte offset (OBDb's bit offset 96, read here as the bytes after the
// `62 70 29` header) and the ECU's answer are the open questions; every reply is logged raw by obdlink.
// The 29-bit priority byte is the ELM's default (ATCP 18, the value after ATZ), relied on, not sent.
// Read-only: three exact commands, each in obd::allowed(): point the header at the meter ECU
// (29-bit physical address DA60F1), the read itself, then the functional header back (DB33F1) so the
// mode 01 polls reach the engine ECU again. Only asked on a 29-bit bus (ATDPN 7), rarely, and given up
// for the session after MAX_FAILS misses in a row. PURE, host-tested.
// Spec: .claude/skills/obd2/references/honda-city-2022.md
namespace fuel22 {
constexpr const char* SET_METER = "ATSH DA60F1\r";     // the meter ECU's physical address on a 29-bit bus
constexpr const char* ASK = "227029\r";                // read data by identifier 7029: the tank, litres
constexpr const char* SET_DEFAULT = "ATSH DB33F1\r";   // the functional address again (the ELM's default)
constexpr float MAX_L = 50.0f;                         // the City's tank is 44 L: more than this is junk
constexpr uint32_t FIRST_MS = 15000, EVERY_MS = 60000; // after the link is live, then this often
constexpr uint32_t FRESH_MS = 300000;                  // a level counts for the range this long
constexpr int MAX_FAILS = 3;                           // misses in a row before it stops for the session

// ATDPN's answer: "A7" (auto, protocol 7) or "7": ISO 15765-4 CAN 29-bit 500 kbit/s. "A0" (no bus found
// yet), 6 (11-bit) and the rest are false.
bool protocol29(const char* atdpnRaw);
// A 227029 reply ('\n' between lines, multi-frame lines "0: 62 70 ..", the 3-digit length line dropped) ->
// litres from the 16 bits at bytes 12-13 after "62 70 29", / 100. False (and *litres untouched) for NO DATA,
// a negative reply, a short or cut-off one, or a value outside 0..MAX_L.
bool parse(const char* raw, float* litres);
// The level while it is fresh (FRESH_MS), else NAN.
float freshLitres(float litres, uint32_t atMs, uint32_t nowMs);

// When to ask. start() once per link-up, after the first live reply and ATDPN; due() then says when;
// done() with how it went.
class Plan {
public:
    void start(uint32_t nowMs, bool bus29);
    bool due(uint32_t nowMs) const;
    void done(uint32_t nowMs, bool ok);
    bool off() const { return off_; }
private:
    bool started_ = false, off_ = false;
    int fails_ = 0;
    uint32_t nextAt_ = 0;
};
}  // namespace fuel22
