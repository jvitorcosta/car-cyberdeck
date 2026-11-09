#pragma once
#include <cstdint>
// When a keep-alive to the black deck (music mode's, karaoke's) asks: on=1 at once when it is wanted,
// then each time its pace (nextAt) comes round, and also at once when the light it carries changed
// (karaoke's colour follows the INSIDE light within a moment, not 10 s); on=0 once when it stops
// being wanted. A keep-alive that carries no light passes -1 always. PURE, host-tested.
namespace keepalive {

struct State {
    bool asked = false;    // an on=1 went out since the last on=0
    uint32_t nextAt = 0;   // millis() of the next on=1
    int sentLight = -2;    // the light the last on=1 carried; -2: none sent
};

enum class Ask : uint8_t { None, On, Off };

Ask due(const State& s, bool want, uint32_t nowMs, int light);

}  // namespace keepalive
