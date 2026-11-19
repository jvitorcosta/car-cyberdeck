#include "daynight.h"

namespace daynight {

bool isNight(int m) { return m >= NIGHT_START || m < NIGHT_END; }

bool isQuiet(int m) {
    return m >= QUIET_START && m < QUIET_END;  // within one day; override with start < end
}

bool synced(int64_t utcSeconds) { return utcSeconds >= SYNCED_AFTER; }

}
