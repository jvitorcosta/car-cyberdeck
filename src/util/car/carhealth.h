#pragma once
#include <cstddef>
#include <cstdint>
#include <cmath>
// The car health check's instant side: what the Watch can say about the battery and the engine from the
// live readings, a hold timer, the trip's LTFT mean and the last trip's outcome (a small Memo the deck
// keeps in NVS, since the power cuts at ignition off before a trip can be judged). One table of
// defaults; the screen turns a Verdict into a colour and a line. Unknown readings (NAN / -1) never
// warn, and "nothing read yet" is not "Ok": see Verdict::known. PURE, host-tested.
namespace carhealth {
enum class Level : uint8_t { Ok, Note, Problem };
enum class Item : uint8_t { Battery, Engine, COUNT };

struct Reading {  // NAN / -1 = unknown
    uint32_t nowMs = 0;
    bool engineRunning = false;
    float volts = NAN;  // ATRV
    float coolantC = NAN, ltftPct = NAN;
    int storedCodes = -1, pendingCodes = -1;
    bool mil = false;
};

struct Verdict {
    Level level = Level::Ok;
    // Something was actually checked. Battery: a volts reading this power-on, or the last trip's low
    // battery. Engine: a trouble-code read (codesRead()), or a judged trip's fuel trim. Never false
    // for a Note or a Problem. An Ok that isn't known means "still checking", not "fine".
    bool known = false;
    char why[48] = "";  // "" when Ok; plain words, caps, <= 47 chars
};

// What the trip so far needs to be judged after a power cut: saved by the deck as it goes, restored
// at the next boot.
struct Memo {
    bool battLowSeen;  // the battery Problem fired this trip
    float ltftSum;     // LTFT sum and count of this trip so far
    uint32_t ltftN;
};
constexpr size_t MEMO_BYTES = 9;  // NVS: flags u8 (bit 0 battLowSeen), ltftSum f32 LE, ltftN u32 LE
void encode(const Memo& m, uint8_t out[MEMO_BYTES]);
// False on a wrong length, unknown flag bits or a non-finite sum; *m is then untouched.
bool decode(const uint8_t* in, size_t len, Memo* m);
// The Memo holds something the next boot must judge (a low battery, fuel-trim samples): worth saving
// even when the car never moved.
inline bool worthSaving(const Memo& m) { return m.battLowSeen || m.ltftN > 0; }

constexpr float BATT_LOW_V = 12.0f;
constexpr uint32_t BATT_HOLD_MS = 60000;
constexpr float COOLANT_HOT_C = 105.0f;
constexpr float LTFT_NOTE = 10.0f, LTFT_PROBLEM = 20.0f;

class Watch {
public:
    void feed(const Reading& r);  // ~1 Hz
    Verdict item(Item i) const;   // the worst live condition for the item
    bool critical() const;        // coolant hot or MIL: may draw while moving
    // The trip is over: LTFT-for-a-whole-trip is judged here, and a battery Problem this trip becomes
    // the Note "BATTERY WAS LOW LAST TRIP". Both stay until the next trip ends; live readings still win.
    void endTrip();
    void codesRead();             // the caller read 03/07 successfully: the engine has been checked
    Memo memo() const;            // this trip so far
    void restore(const Memo& m);  // at boot, before endTrip(): the last trip's state
private:
    Reading last_;
    bool haveLast_ = false;
    bool lowActive_ = false;  // volts under BATT_LOW_V while running, since lowSinceMs_
    uint32_t lowSinceMs_ = 0;
    float ltftSum_ = 0.0f;  // sum and count of the running trip so far
    uint32_t ltftN_ = 0;
    Verdict ltftVerdict_;  // the last finished trip's, kept until the next one ends
    bool ltftJudged_ = false;   // a trip with LTFT readings has been judged
    bool battLowSeen_ = false;  // the battery Problem fired this trip
    bool lastBattLow_ = false;  // ... and in the last finished one
    bool voltsSeen_ = false;    // a volts reading this power-on
    bool codesRead_ = false;
};
}  // namespace carhealth
