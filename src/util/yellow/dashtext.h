#pragma once
// The texts of the yellow deck's main screen (the dashboard in ledpanel.cpp): the date under the
// clock, in capitals. Writes at most n bytes, always terminated. PURE, host-tested.
#include <cstddef>

namespace dashtext {

// The day of the week of a Gregorian date: 0 Sunday .. 6 Saturday.
int weekday(int y, int month, int day);
// "FRI 23 OCT" (English three-letter day and month); "--- -- ---" before the clock synced (!known)
// or for a month or day out of range.
void date(bool known, int y, int month, int day, char* out, size_t n);

}  // namespace dashtext
