#pragma once
#include <cstddef>
// Outside weather for the companion's top strip, from Open-Meteo (free, no key). PURE.
namespace weather {
enum class Icon { Sun, Moon, Cloud, Rain, Storm };
struct Now { double tempC; int code; bool day; };
// Reads the "current" block (current_units repeats the keys with string values first).
bool parse(const char* body, Now* out);
Icon iconFor(int wmoCode, bool day);
void url(const char* lat, const char* lon, char* out, size_t n);
}
