#pragma once
#include <cstddef>
#include <cstdint>
// A camera mark as the JSON of a GitHub issue (black/car/markpush): title "Mark <local time>", body
// "lat: / lon: / heading: / kmh: / time:" lines (open-radars-manaus tools/marks.py reads them) and a
// Google Maps link. local and utc are the formatted times ("" when the clock wasn't set); nothing
// else is free text, so no escaping is needed beyond the line breaks. Returns snprintf's length.
// PURE, host-tested.
namespace markissue {
void microDegrees(char* out, size_t n, int32_t e6);   // micro-degrees as "-2.965692"
int json(char* out, size_t n, const char* local, const char* utc, int32_t latE6, int32_t lonE6,
         uint16_t course, uint16_t kmh);
}  // namespace markissue
