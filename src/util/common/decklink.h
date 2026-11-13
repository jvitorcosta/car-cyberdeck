#pragma once
// The link between the two decks and the GPS state it carries, by the same rules on both: the yellow
// deck asks the black deck's GET /link every CHECK_MS, and each deck counts the link as up while it
// has had a valid exchange with the other within LINK_MS (three checks: one missed poll, the black
// deck's background scan drops about one a minute, doesn't flicker it). /link's body says whether
// the black deck's GPS (the iPhone's, over GPS 2 IP) has a fix; the yellow deck knows the GPS only
// through it, so without a link its GPS is Unknown. Tracker turns what the yellow deck's header
// shows into its terminal messages. PURE, host-tested.
#include <cstddef>
#include <cstdint>

namespace decklink {

constexpr uint32_t LINK_MS = 15000, CHECK_MS = 5000;
// A stamp up to AHEAD_MS after now still counts as recent (see linked).
constexpr uint32_t AHEAD_MS = 1000;

enum class Gps : uint8_t { Unknown, Fix, NoFix };

// heardAt: millis() of the last valid exchange, 0 = never (callers store millis() | 1, so a real
// time is never 0). millis() wrap-safe. A heardAt up to AHEAD_MS after now is linked too: the | 1
// can put it 1 ms ahead, and on the black deck the web task can stamp it after loop() read now.
bool linked(uint32_t heardAt, uint32_t now);
// /link's body: "gps=1" with a fix, "gps=0" without one. Cut to n bytes.
void body(bool fix, char* out, size_t n);
// "gps=1" Fix, "gps=0" NoFix (a trailing newline or space is fine), anything else Unknown.
Gps parse(const char* body);
// What the yellow deck shows: Unknown without a link, else the last answer's.
Gps shown(bool isLinked, Gps last);

extern const char* const LINKED_MSG;  // ">DECK: LINKED"
extern const char* const NOLINK_MSG;  // ">DECK: NO LINK"
extern const char* const FIX_MSG;     // ">GPS: FIX"
extern const char* const NOFIX_MSG;   // ">GPS: NO FIX"
// A message that reports a problem (drawn in ALERT): NOLINK_MSG and NOFIX_MSG.
bool isAlert(const char* msg);

// The yellow deck's link and GPS messages: one update() per loop pass with what the header shows;
// it returns the message to show now, or nullptr. The first known state of each is silent (a deck
// that boots linked says nothing). A link drop is silent while ">BLACK DECK NOT ANSWERING" showed
// under LINK_MS ago (notAnsweringAt: its millis() | 1, 0 = never): that already said it. GPS
// messages only while linked; Unknown is no change. When the link and the GPS change in one pass,
// the link's message comes now and the GPS's on the next pass.
class Tracker {
public:
    const char* update(bool isLinked, Gps g, uint32_t now, uint32_t notAnsweringAt);

private:
    int8_t link_ = -1, gps_ = -1;  // -1: not known yet; else 0/1 (gps_: 1 = fix)
};

}  // namespace decklink
