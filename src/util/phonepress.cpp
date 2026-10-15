#include "phonepress.h"
#include <cstring>

namespace phonepress {
menunav::Button parse(const char* name) {
    if (!name) return menunav::None;
    static const struct { const char* n; menunav::Button b; } MAP[] = {
        {"lyrics", menunav::Lyrics}, {"sounds", menunav::Sounds}, {"trip", menunav::Trip},
        {"back", menunav::Back},     {"howl", menunav::Howl},     {"vroom", menunav::Vroom},
        {"reset", menunav::Reset},
    };
    for (const auto& m : MAP)
        if (strcmp(name, m.n) == 0) return m.b;
    return menunav::None;
}
}
