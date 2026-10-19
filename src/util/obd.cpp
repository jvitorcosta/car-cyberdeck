#include "obd.h"
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace obd {
namespace {
struct Def { const char* cmd; uint8_t pid; uint8_t bytes; };
// Order matches Pid. pid/bytes are unused for Battery (ATRV).
constexpr Def DEFS[(int)Pid::COUNT] = {
    {"010C\r", 0x0C, 2}, {"010D\r", 0x0D, 1}, {"0105\r", 0x05, 1}, {"0104\r", 0x04, 1},
    {"010F\r", 0x0F, 1}, {"0111\r", 0x11, 1}, {"ATRV\r", 0, 0},
};

bool contains(const char* s, const char* word) { return strstr(s, word) != nullptr; }

int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = (char)toupper((unsigned char)c);
    return c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

float value(Pid p, const uint8_t* b) {
    switch (p) {
    case Pid::Rpm: return (256.0f * b[0] + b[1]) / 4.0f;
    case Pid::Speed: return b[0];
    case Pid::Coolant:
    case Pid::Intake: return (float)b[0] - 40.0f;
    case Pid::Load:
    case Pid::Throttle: return b[0] * 100.0f / 255.0f;
    default: return 0.0f;
    }
}

// One line, hex digits only (spaces dropped, upper case): finds "41"+PID and reads the bytes
// after it. False when the line has no such reply or it is cut off.
bool fromLine(const char* line, size_t len, Pid p, float* out) {
    char hex[96];
    size_t n = 0;
    for (size_t i = 0; i < len && n + 1 < sizeof hex; ++i) {
        if (line[i] == ' ') continue;
        if (hexVal(line[i]) < 0) return false;            // not a data line (SEARCHING..., ELM327)
        hex[n++] = (char)toupper((unsigned char)line[i]);
    }
    hex[n] = '\0';
    char want[5];
    static const char* digits = "0123456789ABCDEF";
    const uint8_t pid = DEFS[(int)p].pid;
    want[0] = '4'; want[1] = '1'; want[2] = digits[pid >> 4]; want[3] = digits[pid & 15]; want[4] = 0;
    const char* at = strstr(hex, want);
    if (!at) return false;
    at += 4;
    uint8_t b[2];
    for (int k = 0; k < DEFS[(int)p].bytes; ++k) {
        const int hi = hexVal(at[2 * k]), lo = hi < 0 ? -1 : hexVal(at[2 * k + 1]);
        if (hi < 0 || lo < 0) return false;               // cut off
        b[k] = (uint8_t)(hi << 4 | lo);
    }
    *out = value(p, b);
    return true;
}

Reply battery(const char* raw, float* out) {
    for (const char* s = raw; *s; ++s) {
        if (!isdigit((unsigned char)*s)) continue;
        char* end = nullptr;
        const float v = strtof(s, &end);
        while (end && *end == ' ') ++end;
        if (end && (*end == 'V' || *end == 'v')) { *out = v; return Reply::Ok; }
        s = end ? end - 1 : s;
    }
    return Reply::Error;
}
}

const char* request(Pid p) { return (int)p < (int)Pid::COUNT ? DEFS[(int)p].cmd : "ATRV\r"; }

Reply parse(Pid p, const char* raw, float* value) {
    if (!raw) return Reply::Error;
    if (p == Pid::Battery) return battery(raw, value);
    if (contains(raw, "UNABLE TO CONNECT") || contains(raw, "CAN ERROR") || contains(raw, "BUS ERROR") ||
        (contains(raw, "BUS INIT") && contains(raw, "ERROR")))
        return Reply::NoEngine;
    if (contains(raw, "NO DATA")) return Reply::NoData;
    for (const char* line = raw; *line;) {
        const char* end = strchr(line, '\n');
        const size_t len = end ? (size_t)(end - line) : strlen(line);
        float v;
        if (fromLine(line, len, p, &v)) { *value = v; return Reply::Ok; }
        if (!end) break;
        line = end + 1;
    }
    return Reply::Error;
}

bool allowed(const char* cmd) {
    if (!cmd) return false;
    size_t n = strlen(cmd);
    if (n > 0 && cmd[n - 1] == '\r') --n;                // at most one trailing '\r'
    if (n == 4 && cmd[0] == '0' && cmd[1] == '1') {      // mode 01, one PID
        for (size_t i = 2; i < 4; ++i)
            if (!((cmd[i] >= '0' && cmd[i] <= '9') || (cmd[i] >= 'A' && cmd[i] <= 'F'))) return false;
        return true;
    }
    static const char* const SETUP[] = {"ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATH1", "ATSP0", "ATRV",
                                        "ATI", "ATDPN"};
    for (const char* s : SETUP)
        if (strlen(s) == n && strncmp(cmd, s, n) == 0) return true;
    return false;
}

bool looksLikeAdapter(const char* btName) {
    if (!btName || !btName[0]) return false;
    char up[40];
    size_t n = 0;
    for (; btName[n] && n + 1 < sizeof up; ++n) up[n] = (char)toupper((unsigned char)btName[n]);
    up[n] = '\0';
    return contains(up, "OBD") || contains(up, "ELM") || contains(up, "VLINK") || contains(up, "V-LINK");
}
}
