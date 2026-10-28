#include "intropick.h"
#include <cstring>

namespace intropick {

static bool available(int o, bool haveKiss, bool haveMcQueen, bool haveMax) {
    return o == HONDA || (o == KISS && haveKiss) || (o == MCQUEEN && haveMcQueen) ||
           (o == MAX && haveMax);
}

Opening pick(uint32_t rnd, int last, bool haveKiss, bool haveMcQueen, bool haveMax) {
    Opening list[COUNT];
    int n = 0;
    for (int o = 0; o < COUNT; ++o)
        if (available(o, haveKiss, haveMcQueen, haveMax)) list[n++] = (Opening)o;
    if (n > 1) {                       // drop last boot's, when there's another
        int k = 0;
        for (int i = 0; i < n; ++i)
            if (list[i] != last) list[k++] = list[i];
        n = k;
    }
    return list[rnd % (uint32_t)n];
}

Greeting greetingFor(Opening o) { return o == HONDA ? MAIN : o == MCQUEEN ? ALT : NONE; }

bool isVideo(Opening o) { return o == KISS || o == MAX; }

const char* name(Opening o) {
    static const char* const NAMES[COUNT] = {"kiss", "honda", "mcqueen", "max"};
    return o < COUNT ? NAMES[o] : "?";
}

int parse(const char* s) {
    if (!s) return -1;
    for (int o = 0; o < COUNT; ++o)
        if (strcmp(s, name((Opening)o)) == 0) return o;
    return -1;
}

int forced(int f, bool haveKiss, bool haveMcQueen, bool haveMax) {
    return f >= 0 && f < COUNT && available(f, haveKiss, haveMcQueen, haveMax) ? f : -1;
}

}
