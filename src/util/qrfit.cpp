#include "qrfit.h"

namespace qrfit {

int version(size_t bytes) {
    static const size_t CAPACITY[MAX_VERSION + 1] = {0, 17, 32, 53};   // byte mode, ECC L
    if (bytes == 0) return 0;
    for (int v = 1; v <= MAX_VERSION; ++v)
        if (bytes <= CAPACITY[v]) return v;
    return 0;
}

}
