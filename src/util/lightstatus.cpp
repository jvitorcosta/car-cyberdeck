#include "lightstatus.h"
#include <cstdio>
#include <cstring>

namespace lightstatus {

bool isAlert(const char* msg) {
    static const char SUFFIX[] = ": NO LINK";
    if (!msg || msg[0] != '>') return false;
    const size_t n = strlen(msg), s = sizeof(SUFFIX) - 1;
    return n > s && strcmp(msg + n - s, SUFFIX) == 0;
}

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

bool link(bool inUp, bool outUp, char* buf, size_t n) {
    if (inUp && outUp) snprintf(buf, n, "LINK 2/2");
    else if (inUp) snprintf(buf, n, "OUTSIDE: NO LINK");
    else if (outUp) snprintf(buf, n, "INSIDE: NO LINK");
    else snprintf(buf, n, "NO LINK");
    return !(inUp && outUp);
}

void applied(const char* presetName, char* buf, size_t n) { snprintf(buf, n, ">%s APPLIED", presetName); }

void linkChange(bool inside, bool up, char* buf, size_t n) {
    snprintf(buf, n, ">%s: %s", inside ? "INSIDE" : "OUTSIDE", up ? "LINKED" : "NO LINK");
}

void linkChangeBoth(bool up, char* buf, size_t n) { snprintf(buf, n, ">LIGHTS: %s", up ? "LINKED" : "NO LINK"); }

void notConnected(bool inMissed, bool outMissed, char* line, size_t n) {
    if (!inMissed && !outMissed) return;
    const size_t len = strlen(line);
    if (len + 1 >= n) return;
    const char* which = inMissed && outMissed ? "lights" : inMissed ? "inside" : "outside";
    snprintf(line + len, n - len, ", %s not connected", which);
}

void reply(State in, State out, char* buf, size_t n) { reply(in, out, nullptr, buf, n); }

void reply(State in, State out, const char* fw, char* buf, size_t n) {
    if (fw && fw[0]) snprintf(buf, n, "inside=%s outside=%s fw=%s", word(in), word(out), fw);
    else snprintf(buf, n, "inside=%s outside=%s", word(in), word(out));
}

}  // namespace lightstatus
