#include "lightstatus.h"
#include <cstdio>

namespace lightstatus {

State of(bool linked, bool lit) {
    if (!linked) return State::NoLink;
    return lit ? State::On : State::Off;
}

const char* word(State s) {
    switch (s) {
    case State::NoLink: return "nolink";
    case State::Off:    return "off";
    case State::On:     return "on";
    }
    return "nolink";
}

// One light's part of the line: what the strip is doing, or "--" when the deck can't reach it.
static void part(const ledlook::Look& l, bool up, char* out, size_t n) {
    switch (of(up, l.lit)) {
    case State::On:     snprintf(out, n, "ON %d%%", (int)l.pct); break;
    case State::Off:    snprintf(out, n, "OFF"); break;
    case State::NoLink: snprintf(out, n, "--"); break;
    }
}

void line(const ledlook::Look& in, bool inUp, const ledlook::Look& out, bool outUp, char* buf, size_t n) {
    char inPart[16], outPart[16];
    part(in, inUp, inPart, sizeof inPart);
    part(out, outUp, outPart, sizeof outPart);
    snprintf(buf, n, "IN %s  OUT %s  LINK %d/2", inPart, outPart, (int)inUp + (int)outUp);
}

void applied(const char* presetName, char* buf, size_t n) { snprintf(buf, n, ">%s APPLIED", presetName); }

void linkChange(bool inside, bool up, char* buf, size_t n) {
    snprintf(buf, n, ">%s: %s", inside ? "INSIDE" : "OUTSIDE", up ? "LINKED" : "NO LINK");
}

void linkChangeBoth(bool up, char* buf, size_t n) { snprintf(buf, n, ">LIGHTS: %s", up ? "LINKED" : "NO LINK"); }

void reply(State in, State out, char* buf, size_t n) {
    snprintf(buf, n, "inside=%s outside=%s", word(in), word(out));
}

}  // namespace lightstatus
