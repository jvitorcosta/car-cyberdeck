#include "termlayout.h"
#include <cstring>

namespace termlayout {

int stripLayout(char* line, int x0, const bool chip[2], StripPart out[2]) {
    using termfont::Face;
    char* halves[2] = {line, strstr(line, "  ")};
    if (!halves[1]) return -1;
    *halves[1] = 0;
    halves[1] += 2;
    int x = x0;
    for (int i = 0; i < 2; i++) {
        if (i) x += STRIP_LIGHT_GAP;
        StripPart& p = out[i];
        char* state = strchr(halves[i], ' ');
        if (state) *state++ = 0;
        p.name = halves[i];
        p.state = state;
        p.nameX = x;
        x += termfont::advance(Face::SMALL, p.name);
        if (state && chip[i]) {
            p.chipX = x + 1;  // 2 px clear of the name's ink and of the state's
            x += STRIP_CHIP_SLOT;
        } else {
            p.chipX = -1;
            x += termfont::cell(Face::SMALL);  // a space
        }
        p.stateX = x;
        if (state) x += termfont::advance(Face::SMALL, state);
    }
    return x;
}

}  // namespace termlayout
