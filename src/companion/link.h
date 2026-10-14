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
// Buttons on the companion ask the new deck for something (POST, same REMOTE_TOKEN).
enum class Action : uint8_t { None, Howl, Vroom, TripReset };
// Ok: 2xx. Busy: the deck refused for now (409 busy, 429 too soon, 503 low memory).
// Failed: no answer or any other code.
enum class Result : uint8_t { Idle, Pending, Ok, Busy, Failed };
void request(Action a);              // UI loop; replaces one still waiting
// Ok / Busy / Failed are reported once (with the action), then Idle; Pending while waiting.
Result takeResult(Action* which);
void start();                  // network task on core 0
void snapshot(View& out);      // copy under the lock (UI loop)
}
