#pragma once
#include <cstddef>
#include <cstdint>
// What the car-health trip keeps of a drive: one row per minute (for the trip log and the PC
// checkup), a trip summary (km, litres, km/L), and the battery's first five minutes after the start
// (does the alternator charge?). Fed one sample per second; distance integrates the real time
// between samples. A reading the link no longer refreshes adds time but no km and no litres.
// Unknown readings are NAN and stay out of every average. PURE, host-tested.
namespace tripsum {
struct Sample {  // one per second from the loop; NAN = unknown
    uint32_t nowMs;
    float kmh, rpm, litres /*this second*/, volts, coolantC, trimS, trimL;
    bool fresh;  // the link's readings are fresh (obdplan::staleAfterMs)
};
struct Row {  // one minute
    uint16_t minute;
    float km, avgKmh, maxKmh, avgRpm, litres;
    uint16_t idleS;
    float vMin, vAvg, vMax, coolMax, trimS, trimL;
    uint8_t flags;  // bit0 MIL, bit1 new stored code, bit2 new pending code
};
struct Summary {
    float km, litres, kmL /*NAN if no fuel counted*/, avgKmh;
    uint32_t seconds, idleS;
};
struct Recharge {  // first 5 min after the engine starts
    float peakV;   // NAN until a voltage was read in the window
    uint16_t secondsOver13_8;
    bool done;
};

class Trip {
public:
    // The first sample only sets the clock. A step longer than 3 s (a stalled loop) counts as 3 s.
    void add(const Sample& s);
    // The code flags (Row::flags bits) seen now; they are kept in the minute that is running.
    void orFlags(uint8_t f) { cur_.flags = static_cast<uint8_t>(cur_.flags | f); }
    bool rowReady() const { return ready_; }
    // The last closed minute; if several closed before a take, only the latest. A minute without one
    // fresh sample (no adapter, link down) closes without a row but still takes its minute number.
    Row takeRow();
    Summary summary() const;
    Recharge recharge() const;

private:
    struct Acc {  // the minute being built
        uint32_t ms = 0;
        double km = 0, litres = 0, kmhSum = 0, rpmSum = 0, vSum = 0, tsSum = 0, tlSum = 0;
        uint32_t kmhN = 0, rpmN = 0, vN = 0, tsN = 0, tlN = 0, idleMs = 0;
        uint32_t freshN = 0;  // fresh samples: none, and the minute is no row
        float maxKmh = 0, vMin = 0, vMax = 0, coolMax = 0;
        bool hasMax = false, hasV = false, hasCool = false;
        uint8_t flags = 0;
    };
    void closeRow();

    bool started_ = false;
    uint32_t lastMs_ = 0;
    uint32_t totalMs_ = 0, idleMs_ = 0;
    double km_ = 0, litres_ = 0;
    Acc cur_;
    uint16_t nextMinute_ = 0;
    bool ready_ = false;
    Row row_{};
    // recharge window
    bool engineSeen_ = false;
    uint32_t winMs_ = 0, overMs_ = 0;
    float peakV_ = 0;
    bool hasPeak_ = false;
};

// One line of JSON plus '\n' in out (NUL-terminated); returns its length, or 0 if it doesn't fit in n.
// NAN goes out as null. tripId is copied without quotes, backslashes and control characters.
size_t jsonRow(const Row& r, const char* tripId, char* out, size_t n);
}  // namespace tripsum
