#pragma once
#include <cstdint>
#include "../util/stateparse.h"
// The companion's network task: polls the new deck's GET /state (plain HTTP on the hotspot)
// about once a second, every 5 s while it doesn't answer (util/pollpace), and keeps WiFi up.
namespace decklink {
struct View {
    bool deckOk;               // the last poll answered
    bool wrongKey;             // the deck answered 403: REMOTE_TOKEN differs
    uint32_t polledAtMs;       // millis() of the last good answer (0 = none yet)
    stateparse::Deck deck;     // last good state
};
void start();                  // network task on core 0
void snapshot(View& out);      // copy under the lock (UI loop)
}
