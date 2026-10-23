#pragma once
// The texts of the yellow deck's main screen (the dashboard in ledpanel.cpp): the date under the
// clock, the outside temperature, and today's high / low with the city. Upper case where the
// terminal shows capitals; the font is ASCII only, so names are accent-folded. Each writes at most n
// bytes, always terminated. PURE, host-tested.
#include <cstddef>

namespace dashtext {

// The day of the week of a Gregorian date: 0 Sunday .. 6 Saturday.
int weekday(int y, int month, int day);
// "FRI 23 OCT" (English three-letter day and month); "--- -- ---" before the clock synced (!known)
// or for a month or day out of range.
void date(bool known, int y, int month, int day, char* out, size_t n);
// "32C", rounded; "--C" before the first reading. (No degree sign: the font is ASCII.)
void temp(bool ok, double c, char* out, size_t n);
// "HI 32 LO 26  MANAUS": today's max and min rounded ("--" each without them), then the city in
// capitals, folded; without a city just "HI 32 LO 26".
void hiLo(bool daily, double maxC, double minC, const char* city, char* out, size_t n);

}  // namespace dashtext
