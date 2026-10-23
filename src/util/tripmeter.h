#pragma once
#include <cstddef>
#include <cstdint>
// The car-LED deck's trip meter: time since power-on, and distance once a speed source feeds it (the
// OBD adapter, later). Integration, text and the NVS record. PURE, host-tested.
namespace tripmeter {
struct Trip {
    uint32_t seconds = 0;
    uint32_t meters = 0;
    bool hasDistance = false;  // false until a speed was added: the line shows "--" rather than a made-up 0.0
};

class Meter {
public:
    // A speed held for dtMs. 1 km/h for 1 ms is 1/3600 m; the leftover is carried, not rounded away,
    // so a fast poll of slow speeds doesn't lose distance.
    void addSpeed(uint16_t kmh, uint32_t dtMs);
    void setSeconds(uint32_t s) { seconds_ = s; }
    Trip trip() const;
private:
    uint32_t meters_ = 0;
    uint32_t rem_ = 0;  // kmh*ms not yet a whole metre (< 3600)
    uint32_t seconds_ = 0;
    bool hasDistance_ = false;
};

// "TRIP 12.4 KM  0:32"; no distance: "TRIP -- KM  0:32". Tenths round down; hours aren't capped.
void line(const Trip& t, char* buf, size_t n);
// ">LAST TRIP 12.4 KM 0:47": the boot message for the trip before, fits the deck's 32-byte buffer.
void last(const Trip& t, char* buf, size_t n);

// The NVS record: seconds u32 LE, meters u32 LE, hasDistance u8. decode() is false (and *t = Trip{})
// on a wrong length or a flag above 1, so a stale or foreign blob can't show nonsense.
constexpr size_t RECORD_BYTES = 9;
void encode(const Trip& t, uint8_t out[RECORD_BYTES]);
bool decode(const uint8_t* in, size_t len, Trip* t);
}
