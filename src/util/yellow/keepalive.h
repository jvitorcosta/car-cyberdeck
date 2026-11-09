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

// What the prompt line says after a tap switched karaoke on, so the tap doesn't seem to do nothing
// while the black deck is slow to answer (an ask that times out waits 1.5 s, the next one 2 s
// later): Connected at the first 200, NoAnswer once after NO_ANSWER_MISSES misses in a row (it
// keeps waiting: a later 200 is still Connected), None otherwise and without a tap.
constexpr int NO_ANSWER_MISSES = 2;
enum class Shown : uint8_t { None, Connected, NoAnswer };
struct Confirm {
    bool waiting = false;
    int misses = 0;
};
void start(Confirm& c);
Shown answer(Confirm& c, int rc);

}  // namespace keepalive
