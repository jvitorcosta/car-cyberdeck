#pragma once
#include <cstddef>
#include <cstdint>
#include "cameras.h"  // Fix only: the reader hands the matcher its input and knows nothing else of it
// NMEA reader for the GPS 2 IP phone app (TCP, about 1 Hz): bytes in one at a time, a Fix out for each
// complete $..RMC sentence with a good checksum. Own ~100-line parser instead of a library: no
// allocation, a fixed line buffer, and it accepts any talker ($GPRMC, $GNRMC, $GARMC...). PURE, host-tested.
namespace nmea {
class Reader {
public:
    // True when a sentence was completed by this byte and *out holds it. valid = status 'A'; with
    // status 'V' it is still true but valid = false (the phone talks, it just has no fix) and the
    // position/speed are unused. Speed is knots -> km/h rounded down; an empty course keeps the
    // previous one; an empty speed is 0. out->ms is the ms of the call that completed the sentence.
    bool feed(char c, uint32_t ms, cameras::Fix* out);

private:
    static constexpr size_t LINE_MAX = 120;  // an RMC line is ~70 bytes; longer is garbage
    char line_[LINE_MAX];
    size_t len_ = 0;
    bool inLine_ = false;
    uint16_t course_ = 0;
    bool parse(uint32_t ms, cameras::Fix* out);
};
}  // namespace nmea
