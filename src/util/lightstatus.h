#pragma once
// What the car light panel tells the watch, per light: is it on, and is the deck linked to it at
// all. Two words of reply each ("on", "off", "nolink"), the Lights page's link line and the
// terminal messages. PURE, host-tested.
#include <cstddef>
#include <cstdint>

namespace lightstatus {
// A light the deck can't reach is NoLink whatever its last known state, so the watch never shows
// a stale "on" for a panel it has lost.
enum class State : uint8_t { NoLink, Off, On };
State of(bool linked, bool lit);      // NoLink whenever !linked, whatever lit says
const char* word(State s);            // "nolink", "off", "on"

// The Lights page's status line, just the link state: "LINK 2/2" with both lights linked, else the
// one down, "OUTSIDE: NO LINK" / "INSIDE: NO LINK", or "NO LINK" for both. True when a light is
// down (drawn in ALERT). Each light's colour and level are on the page and the main screen. Cut to
// n bytes.
bool link(bool inUp, bool outUp, char* buf, size_t n);
// Terminal status messages: ">ROSE APPLIED", ">INSIDE: NO LINK", ">OUTSIDE: LINKED".
void applied(const char* presetName, char* buf, size_t n);
void linkChange(bool inside, bool up, char* buf, size_t n);
// Both lights changed together: ">LIGHTS: NO LINK" / ">LIGHTS: LINKED".
void linkChangeBoth(bool up, char* buf, size_t n);
// A message that reports a problem (drawn in the ALERT colour): ">INSIDE: NO LINK" and its kin.
bool isAlert(const char* msg);
// The GET /status reply: "inside=on outside=nolink". Cut to n bytes.
void reply(State in, State out, char* buf, size_t n);
}  // namespace lightstatus
