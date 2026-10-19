#include "obd.h"
#include <cctype>
#include <cstdlib>
#include <cstring>

namespace obd {
namespace {
struct Def { const char* cmd; uint8_t pid; uint8_t bytes; };
// Order matches Pid. pid/bytes are unused for Battery (ATRV). FuelSys reads system 1 only.
constexpr Def DEFS[(int)Pid::COUNT] = {
    {"010C\r", 0x0C, 2}, {"010D\r", 0x0D, 1}, {"0105\r", 0x05, 1}, {"0104\r", 0x04, 1},
    {"010F\r", 0x0F, 1}, {"0111\r", 0x11, 1}, {"ATRV\r", 0, 0},
    {"0149\r", 0x49, 1}, {"014A\r", 0x4A, 1}, {"012F\r", 0x2F, 1}, {"010B\r", 0x0B, 1},
    {"0110\r", 0x10, 2}, {"0152\r", 0x52, 1}, {"0103\r", 0x03, 1},
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
    case Pid::Throttle:
    case Pid::PedalD:
    case Pid::PedalE:
    case Pid::Fuel:
    case Pid::Ethanol: return b[0] * 100.0f / 255.0f;
    case Pid::Map:
    case Pid::FuelSys: return b[0];
    case Pid::Maf: return (256.0f * b[0] + b[1]) / 100.0f;
    default: return 0.0f;
    }
}

// One line, hex digits only (spaces dropped, upper case): finds "41"+pid and reads `bytes` bytes
// after it into b. False when the line has no such reply or it is cut off.
bool reply41(const char* line, size_t len, uint8_t pid, int bytes, uint8_t* b) {
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
    want[0] = '4'; want[1] = '1'; want[2] = digits[pid >> 4]; want[3] = digits[pid & 15]; want[4] = 0;
    const char* at = strstr(hex, want);
    if (!at) return false;
    at += 4;
    for (int k = 0; k < bytes; ++k) {
        const int hi = hexVal(at[2 * k]), lo = hi < 0 ? -1 : hexVal(at[2 * k + 1]);
        if (hi < 0 || lo < 0) return false;               // cut off
        b[k] = (uint8_t)(hi << 4 | lo);
    }
    return true;
}

bool fromLine(const char* line, size_t len, Pid p, float* out) {
    uint8_t b[2];
    if (!reply41(line, len, DEFS[(int)p].pid, DEFS[(int)p].bytes, b)) return false;
    *out = value(p, b);
    return true;
}

// Calls f(line, len) for each '\n'-separated line of raw until it returns true.
template <typename F>
bool anyLine(const char* raw, F f) {
    for (const char* line = raw; *line;) {
        const char* end = strchr(line, '\n');
        const size_t len = end ? (size_t)(end - line) : strlen(line);
        if (f(line, len)) return true;
        if (!end) break;
        line = end + 1;
    }
    return false;
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
    float v;
    if (anyLine(raw, [&](const char* line, size_t len) { return fromLine(line, len, p, &v); })) {
        *value = v;
        return Reply::Ok;
    }
    return Reply::Error;
}

const char* supportRequest(int block) {
    static const char* const REQ[] = {"0100\r", "0120\r", "0140\r", "0160\r"};
    return block >= 0 && block < 4 ? REQ[block] : nullptr;
}

bool parseSupported(int block, const char* raw, uint32_t* bits) {
    if (!raw || block < 0 || block > 3) return false;
    uint8_t b[4];
    if (!anyLine(raw, [&](const char* line, size_t len) {
            return reply41(line, len, (uint8_t)(block * 0x20), 4, b);
        }))
        return false;
    *bits = (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
    return true;
}

bool isSupported(const uint32_t bits[4], uint8_t pid) {
    if (pid == 0 || pid > 0x80) return false;
    const int i = pid - 1;
    return (bits[i / 32] >> (31 - i % 32)) & 1u;
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
