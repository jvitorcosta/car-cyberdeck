#pragma once
#include <cstddef>
#include <cstdint>
// The speed-camera list: data/cameras.csv text -> points in RAM. The deck stores the CSV as-is and
// parses it at boot and after a push; a bad list is refused whole (with the line to fix) rather than
// half loaded, so an alert never comes from a row the owner did not mean. PURE, host-tested.
//
// Columns, in this order (the header line is required): id,kind,lat,lon,heading,limit,label,source,checked
// `source` and `checked` are for the PC tool and the owner; the deck reads them only to count columns.
namespace camlist {
enum class Kind : uint8_t { Speed, Bump, RedLight };
constexpr int MAX_POINTS = 96;
// What the deck stores and accepts. NVS is 20 KB (5 pages, one kept free): a push writes the new
// list before the old one goes, and OTA's `good`, `boot/last` and the WiFi keys live there too, so
// 6 KB (twice that in flight) leaves them room. tools/cameras.py's MAX_BYTES follows it.
constexpr size_t MAX_CSV = 6144;
constexpr uint8_t NO_LIMIT = 0;   // limit unknown (or none, for a red light)
struct Point {
    int32_t latE6, lonE6;  // micro-degrees: integers, so matching never touches float drift
    uint16_t heading;      // 0..359, the enforced direction of travel
    uint8_t limit;         // km/h, or NO_LIMIT
    Kind kind;
    char id[12];
    char label[28];
};

// Parses the whole CSV (header line required, exactly the column order above; CRLF or LF).
// Returns the row count, or -1 with *badLine = 1-based line of the first bad row (header = 1).
// Bad: wrong column count, unknown kind, lat/lon not a number, heading empty or not 0..359,
// limit not empty/1..199, a redlight with a limit, more than max rows. Label cut to fit, UTF-8 safe.
// Blank lines at the end are ignored; a blank line before a row is a bad line.
int parse(const char* csv, size_t len, Point* out, int max, int* badLine);
const char* kindName(Kind k);  // "speed" | "bump" | "redlight"
}  // namespace camlist
