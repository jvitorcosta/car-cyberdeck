#pragma once
// What the car light panel tells the watch, per light: is it on, and is the deck linked to it at
// all. Two words of reply each ("on", "off", "nolink") and the Lights page line. PURE, host-tested.
#include <cstddef>
#include <cstdint>
#include "ledlook.h"

namespace lightstatus {
// A light the deck can't reach is NoLink whatever its last known state, so the watch never shows
// a stale "on" for a panel it has lost.
enum class State : uint8_t { NoLink, Off, On };
State of(bool linked, bool lit);      // NoLink whenever !linked, whatever lit says
const char* word(State s);            // "nolink", "off", "on"

// The Lights page line: "IN ON 50%  OUT --  LINK 1/2". A linked light shows OFF when off and its
// pct when on; an unlinked one shows "--". LINK counts the linked lights out of 2. Cut to n bytes.
void line(const ledlook::Look& in, bool inUp, const ledlook::Look& out, bool outUp, char* buf, size_t n);
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
