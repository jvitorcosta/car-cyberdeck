#include "obdlearn.h"

namespace obdlearn {

float Range::scale(float raw) {
    if (raw < lo_) lo_ = raw;
    if (raw > hi_) hi_ = raw;
    if (!(hi_ > lo_)) return 0.0f;
    return (raw - lo_) * 100.0f / (hi_ - lo_);
}

void StartDip::feed(float volts, Engine engine) {
    if (done_ || std::isnan(volts) || engine == Engine::Unknown) return;
    if (engine == Engine::Off) {
        sawOff_ = true;
        if (volts < min_) min_ = volts;
        return;
    }
    done_ = true;                              // the first start decides, for the whole power-on
    value_ = sawOff_ ? min_ : NAN;
}
}
