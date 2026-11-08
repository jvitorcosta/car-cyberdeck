#include "dashtext.h"
#include <cstdio>

namespace dashtext {

namespace {

const char* const DAYS[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const char* const MONTHS[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

}  // namespace

// Sakamoto's method: January and February count as months of the year before.
int weekday(int y, int month, int day) {
    static const int T[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + T[(month - 1) % 12] + day) % 7;
}

void date(bool known, int y, int month, int day, char* out, size_t n) {
    if (!known || month < 1 || month > 12 || day < 1 || day > 31) {
        snprintf(out, n, "--- -- ---");
        return;
    }
    snprintf(out, n, "%s %d %s", DAYS[weekday(y, month, day)], day, MONTHS[month - 1]);
}

}  // namespace dashtext
