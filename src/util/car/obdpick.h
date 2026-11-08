#pragma once
#include <cstdint>
// Which scanned BLE device the OBD deck tries as the adapter. A neighbour's serial gadget (an HM-10
// on FFE0, an LED strip) must never shadow the Vgate, so the whole scan is weighed: a name that
// looks like an adapter (obd::looksLikeAdapter) wins, the strongest signal among those; only when
// no name matches, a nameless device advertising a serial service (the strongest). The car's lights
// (their MACs, flagged by the caller, or an ELK / MELK name: their controllers serve FFF0 too) are
// never picked, and an address that just failed (see skipAfter) is skipped for SKIP_MS, so the next
// retry tries someone else. PURE, host-tested.
namespace obdpick {
constexpr int RING = 4;                 // failed addresses remembered
constexpr uint32_t SKIP_MS = 600000;    // 10 min

struct Candidate {
    uint64_t addr;
    const char* name;   // "" or nullptr when it advertised none
    int rssi;           // dBm
    bool serial;        // advertises FFF0 / FFE0 / 18F0 (obdble::serialService)
    bool light;         // one of the car's lights by MAC (config.h)
};

class Recent {
public:
    void failed(uint64_t addr, uint32_t nowMs);   // the same address again refreshes its slot
    bool skip(uint64_t addr, uint32_t nowMs) const;
private:
    uint64_t addr_[RING] = {};
    uint32_t atMs_[RING] = {};
    bool used_[RING] = {};
    int next_ = 0;
};

bool isLight(const char* name);   // "ELK..." / "MELK...", any case
// The candidate to try, or -1 for none.
int pick(const Candidate* c, int n, const Recent& recent, uint32_t nowMs);
// Whether a scanned device that failed goes into Recent (by its advertised name). One whose name
// looks like an adapter only after a failed Setup (it connected but didn't answer as an ELM327): a
// first-time Vgate that misses one Connect is tried again next round. A nameless fallback after
// either failure.
enum class Failure { Connect, Setup };
bool skipAfter(const char* advertisedName, Failure f);
}
