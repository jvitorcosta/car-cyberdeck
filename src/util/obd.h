#pragma once
#include <cstddef>
#include <cstdint>
// ELM327 requests and replies for the old deck's live gauges (companion/obdlink): mode 01 PIDs
// and the adapter's own battery reading. Read-only by design: nothing here can send mode 03/04.
// Tolerates what cheap clones wrap around a reply (echo, SEARCHING..., CAN headers, several
// ECUs, spaces or none). PURE, host-tested.
// Spec: docs/superpowers/specs/2026-10-15-obd-gauges-design.md
namespace obd {
enum class Pid : uint8_t { Rpm, Speed, Coolant, Load, Intake, Throttle, Battery, COUNT };
// Ok: *value set. NoData: the car doesn't support this PID. NoEngine: the adapter is up but the
// car doesn't answer (ignition off). Error: anything else (timeout, junk, cut-off reply).
enum class Reply : uint8_t { Ok, NoData, NoEngine, Error };
const char* request(Pid p);          // the command, ending in '\r'
// raw: everything the adapter sent before its '>' prompt. *value is touched only on Ok.
Reply parse(Pid p, const char* raw, float* value);
// The only gate to the car (spec 2026-10-19 "Read-only guarantee"): true for exactly a mode 01
// request ("01" + two upper-case hex digits) or one of the adapter's own setup commands (ATZ, ATE0,
// ATL0, ATS0, ATH0, ATH1, ATSP0, ATRV, ATI, ATDPN), each with at most one trailing '\r'. Anything
// else, any other mode, header or monitor command, lower case, spacing or a second command, is false.
bool allowed(const char* cmd);
// A Bluetooth name that is probably an ELM327 (OBD, ELM, V-LINK / VLINK, any case).
bool looksLikeAdapter(const char* btName);
}
