#include "camtone.h"

namespace camtone {

// Our call (the spec left exact tones to the builder): pitch tells the kind apart without looking.
static constexpr uint16_t MID = 1000, LOW = 600, HIGH = 1600;

static int twoBeeps(uint16_t hz, Note* out) {
    out[0] = {hz, 120};
    out[1] = {0, 80};
    out[2] = {hz, 120};
    return 3;
}

int alarmRepeat(Note* out) {
    int n = 0;
    for (int i = 0; i < 3; i++) {
        if (i) out[n++] = {0, 60};
        out[n++] = {HIGH, 80};
    }
    return n;
}

int onChange(cameras::Stage from, cameras::Stage to, camlist::Kind kind, Note* out) {
    using cameras::Stage;
    if (to == Stage::Alarm && from != Stage::Alarm) return alarmRepeat(out);
    if (to == Stage::Alert && (from == Stage::Off || from == Stage::NoFix)) {
        switch (kind) {
            case camlist::Kind::Speed: return twoBeeps(MID, out);
            case camlist::Kind::Bump: return twoBeeps(LOW, out);
            case camlist::Kind::RedLight: out[0] = {HIGH, 500}; return 1;
        }
    }
    return 0;
}

}  // namespace camtone
