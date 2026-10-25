#include "dashtext.h"
#include <cmath>
#include <cstdio>
#include "text.h"

namespace dashtext {

namespace {

const char* const DAYS[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const char* const MONTHS[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

int rounded(double c) { return static_cast<int>(std::lround(c)); }

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

void temp(bool ok, double c, char* out, size_t n) {
    if (ok) snprintf(out, n, "%d\xB0" "C", rounded(c));  // 0xB0: the degree sign in Latin-1
    else snprintf(out, n, "--\xB0" "C");
}

void hiLo(bool daily, double maxC, double minC, const char* city, char* out, size_t n) {
    char hi[12] = "--", lo[12] = "--";
    if (daily) {
        snprintf(hi, sizeof(hi), "%d", rounded(maxC));
        snprintf(lo, sizeof(lo), "%d", rounded(minC));
    }
    char folded[48] = "";
    if (city) txt::latin1Fold(city, folded, sizeof(folded));
    txt::latin1Upper(folded);
    if (folded[0]) snprintf(out, n, "HI %s LO %s  %s", hi, lo, folded);
    else snprintf(out, n, "HI %s LO %s", hi, lo);
}

}  // namespace dashtext
