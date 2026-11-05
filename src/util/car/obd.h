#pragma once
#include <cstddef>
#include <cstdint>
// ELM327 requests and replies for the yellow deck's car health check: mode 01 PIDs, the
// adapter's own battery reading and the read-only trouble code requests (03 stored, 07 pending).
// Read-only by design: allowed() refuses everything else, mode 04 (clear codes) included.
// Tolerates what cheap clones wrap around a reply (echo, SEARCHING..., CAN headers, several
// ECUs, spaces or none). PURE, host-tested.
// Spec: docs/superpowers/specs/2026-10-28-car-health-check-design.md
namespace obd {
// PedalD/E: accelerator pedal (49/4A); Fuel: tank level (2F); Map: manifold kPa (0B); Maf: air g/s
// (10); Ethanol: % (52); FuelSys: fuel system 1 status (03; 4 = open loop on deceleration: fuel cut);
// CoolantX / IntakeX: the extended coolant / intake readings (67 / 68, sensor 1), all the City has.
// TrimS / TrimL: short / long term fuel trim, % (06 / 07); ModuleV: the ECU's own supply voltage (42);
// Status (01): the stored-code count, plus 1000 when the check-engine lamp (MIL) is on (milOn, codeCount);
// over every ECU's line: the lamp ORed, the counts summed (capped at 127).
enum class Pid : uint8_t { Rpm, Speed, Coolant, Load, Intake, Throttle, Battery, PedalD, PedalE, Fuel,
                           Map, Maf, Ethanol, FuelSys, CoolantX, IntakeX, TrimS, TrimL, ModuleV, Status,
                           COUNT };
// Ok: *value set. NoData: the car doesn't support this PID. NoEngine: the adapter is up but the
// car doesn't answer (ignition off). Error: anything else (timeout, junk, cut-off reply).
enum class Reply : uint8_t { Ok, NoData, NoEngine, Error };
const char* request(Pid p);          // the command, ending in '\r'
// raw: everything the adapter sent before its '>' prompt. *value is touched only on Ok.
Reply parse(Pid p, const char* raw, float* value);
// The supported-PID scan: block 0..3 asks 0100/0120/0140/0160 (nullptr past 3); parseSupported
// reads the 4 bytes after "41 <block*20h>" (bit 31 = PID block*20h + 1), ORed over every ECU's
// line (engine and transmission both answer); isSupported looks a PID
// (01..80h) up in the four blocks read.
const char* supportRequest(int block);
bool parseSupported(int block, const char* raw, uint32_t* bits);
bool isSupported(const uint32_t bits[4], uint8_t pid);
// Status (PID 01) as parse() returns it: stored-code count, +1000 with the check-engine lamp on.
bool milOn(float statusValue);
int codeCount(float statusValue);
// Trouble codes, read-only: "03\r" asks the stored ones, "07\r" the pending ones.
const char* codesRequest(bool pending);
// Reads the reply to codesRequest(pending) ("43 <count> <2 bytes per code>...", "47 ..." for
// pending; every ECU may answer on its own line; "NO DATA" = no codes) into out as "P0301".
// Returns how many were written (at most max, even when the car has more), or -1 for an error,
// junk, a cut-off reply or the other mode's reply.
int parseCodes(const char* raw, bool pending, char (*out)[6], int max);
// The only gate to the car: true for exactly a mode 01 request ("01" + two upper-case hex digits),
// "03" or "07" (read the stored / pending trouble codes), or one of the adapter's own setup
// commands (ATZ, ATE0, ATL0, ATS0, ATH0, ATSP0, ATRV, ATI, ATDPN), each ending in exactly one '\r'. Anything
// else (mode 04 above all), any other mode, header or monitor command, ATH1 (headers on would break
// parse()'s anchors), lower case, spacing or a second command, is false.
bool allowed(const char* cmd);
// The adapter's answer to one of its setup commands is a real one: ATZ's banner ("ELM...", or
// "OK" on some clones), "OK" for the rest. A cut-off banner or a "?" is not.
bool setupOk(const char* cmd, const char* raw);
// A Bluetooth name that is probably an ELM327 (OBD, ELM, V-LINK / VLINK, any case).
bool looksLikeAdapter(const char* btName);
}
