#pragma once
// The texts of the yellow deck's main screen (the dashboard in ledpanel.cpp): the date under the
// clock, the outside temperature, and today's high / low with the city. Upper case where the
// terminal shows capitals. The texts are Latin-1 bytes, as the yellow deck's fonts draw them (the
// city keeps its accents, the temperature has its degree sign). Each writes at most n bytes, always
// terminated. PURE, host-tested.
#include <cstddef>

namespace dashtext {

// The day of the week of a Gregorian date: 0 Sunday .. 6 Saturday.
int weekday(int y, int month, int day);
// "FRI 23 OCT" (English three-letter day and month); "--- -- ---" before the clock synced (!known)
// or for a month or day out of range.
void date(bool known, int y, int month, int day, char* out, size_t n);
// "32°C" (the degree sign as Latin-1 0xB0), rounded; "--°C" before the first reading.
void temp(bool ok, double c, char* out, size_t n);
// "HI 32 LO 26  MANAUS": today's max and min rounded ("--" each without them), then the city in
// capitals (Latin-1: "SÃO PAULO"); without a city just "HI 32 LO 26".
void hiLo(bool daily, double maxC, double minC, const char* city, char* out, size_t n);

}  // namespace dashtext
