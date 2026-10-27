#pragma once
#include <cstddef>
// Outside weather from Open-Meteo (free, no key): the yellow deck's
// main screen. url() asks for the current reading and today's max/min. PURE, host-tested.
namespace weather {
enum class Icon { Sun, Moon, Cloud, Rain, Storm };
struct Now { double tempC; int code; bool day; };
// Reads the "current" block (current_units repeats the keys with string values first).
bool parse(const char* body, Now* out);
// Today's max and min: the first number of the "daily" block's temperature_2m_max / _min arrays
// (daily_units repeats the keys with strings). False when either is missing, empty or null.
bool parseDaily(const char* body, double* maxC, double* minC);
Icon iconFor(int wmoCode, bool day);
// The WMO code as the terminal's words: "CLEAR", "PARTLY CLOUDY", ... "UNKNOWN".
const char* words(int wmoCode);
// Plain http: no TLS on a heap shared with BLE, WiFi and a web server.
void url(const char* lat, const char* lon, char* out, size_t n);
}
