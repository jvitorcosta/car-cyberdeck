#include "obd.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace obd {
namespace {
constexpr float MAX_VOLTS = 20.0f;   // ATRV: a 12 V system reads ~11-15 V
struct Def { const char* cmd; uint8_t pid; uint8_t bytes; };
// Order matches Pid. pid/bytes are unused for Battery (ATRV). FuelSys reads system 1 only; Status
// reads only its first byte (MIL flag and code count).
constexpr Def DEFS[(int)Pid::COUNT] = {
    {"010C\r", 0x0C, 2}, {"010D\r", 0x0D, 1}, {"0105\r", 0x05, 1}, {"0104\r", 0x04, 1},
    {"010F\r", 0x0F, 1}, {"0111\r", 0x11, 1}, {"ATRV\r", 0, 0},
    {"0149\r", 0x49, 1}, {"014A\r", 0x4A, 1}, {"012F\r", 0x2F, 1}, {"010B\r", 0x0B, 1},
    {"0110\r", 0x10, 2}, {"0152\r", 0x52, 1}, {"0103\r", 0x03, 1}, {"0167\r", 0x67, 2},
    {"0168\r", 0x68, 2}, {"0106\r", 0x06, 1}, {"0107\r", 0x07, 1}, {"0142\r", 0x42, 2},
    {"0101\r", 0x01, 1},
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
    case Pid::TrimS:
    case Pid::TrimL: return ((float)b[0] - 128.0f) * 100.0f / 128.0f;
    case Pid::ModuleV: return (256.0f * b[0] + b[1]) / 1000.0f;
    case Pid::Status: return (float)(b[0] & 0x7F) + ((b[0] & 0x80) ? 1000.0f : 0.0f);
    case Pid::CoolantX:
    case Pid::IntakeX: return (b[0] & 1) ? (float)b[1] - 40.0f : NAN;   // A: sensors present
    default: return 0.0f;
    }
}

// Where a reply's data starts: just after marker ("41"+pid, "43", "47"), but only when the marker
// is the line's first byte, or follows a CAN header (7Ex, with ATH1) and optionally its length byte.
// Anywhere else it is just data of some other reply ("43 02 01 47" is not a mode 07 reply), or
// half of two bytes. nullptr when it isn't there. h: hex digits only, upper case.
const char* anchored(const char* h, const char* marker) {
    const size_t m = strlen(marker), n = strlen(h);
    if (strncmp(h, marker, m) == 0) return h + m;
    if (n >= 5 && h[0] == '7' && h[1] == 'E' && hexVal(h[2]) >= 0) {
        if (strncmp(h + 3, marker, m) == 0) return h + 3 + m;
        if (strncmp(h + 5, marker, m) == 0) return h + 5 + m;
    }
    return nullptr;
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
    const char* at = anchored(hex, want);
    if (!at) return false;
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
    return !std::isnan(*out);                            // a reading without the sensor: no value
}

// Calls f(line, len) for each line of raw ('\r' or '\n' ends one: the adapter says '\r') until it
// returns true.
template <typename F>
bool anyLine(const char* raw, F f) {
    for (const char* line = raw; *line;) {
        const size_t len = strcspn(line, "\r\n");
        if (len && f(line, len)) return true;
        line += len;
        line += strspn(line, "\r\n");
    }
    return false;
}

Reply battery(const char* raw, float* out) {
    for (const char* s = raw; *s; ++s) {
        if (!isdigit((unsigned char)*s)) continue;
        char* end = nullptr;
        const float v = strtof(s, &end);
        // Not a number glued to a word ("ELM327 v1.5": a clone's banner after a brown-out
        // reboot) and only a car battery's range.
        const bool word = s > raw && isalpha((unsigned char)s[-1]);
        while (end && *end == ' ') ++end;
        if (end && (*end == 'V' || *end == 'v') && !word && v <= MAX_VOLTS) { *out = v; return Reply::Ok; }
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
    uint32_t all = 0;                                    // every ECU answers with its own line: OR them
    bool any = false;
    anyLine(raw, [&](const char* line, size_t len) {
        uint8_t b[4];
        if (reply41(line, len, (uint8_t)(block * 0x20), 4, b)) {
            all |= (uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 | (uint32_t)b[2] << 8 | b[3];
            any = true;
        }
        return false;                                    // keep reading the other lines
    });
    if (!any) return false;
    *bits = all;
    return true;
}

bool isSupported(const uint32_t bits[4], uint8_t pid) {
    if (pid == 0 || pid > 0x80) return false;
    const int i = pid - 1;
    return (bits[i / 32] >> (31 - i % 32)) & 1u;
}

bool milOn(float statusValue) { return statusValue >= 1000.0f; }
int codeCount(float statusValue) { return (int)(statusValue >= 1000.0f ? statusValue - 1000.0f : statusValue); }

const char* codesRequest(bool pending) { return pending ? "07\r" : "03\r"; }

namespace {
// Hex digits of s[0..len) appended to buf at *n (spaces dropped, upper case). A multi-frame line
// starts "0:", "1:"...; that prefix is dropped and *framed set. False when the line holds anything
// else (SEARCHING..., NO DATA, the "008" length line is hex and harmless): not a data line.
bool lineHex(const char* s, size_t len, char* buf, size_t cap, size_t* n, bool* framed) {
    size_t i = 0;
    *framed = len >= 2 && s[1] == ':';
    if (*framed) i = 2;
    for (; i < len; ++i) {
        if (s[i] == ' ' || s[i] == '\r') continue;
        if (hexVal(s[i]) < 0) return false;
        if (*n + 1 >= cap) return false;
        buf[(*n)++] = (char)toupper((unsigned char)s[i]);
    }
    buf[*n] = '\0';
    return true;
}

char hexDigit(unsigned v) { return "0123456789ABCDEF"[v & 15]; }
}

int parseCodes(const char* raw, bool pending, char (*out)[6], int max) {
    if (!raw || !out || max < 0) return -1;
    const char* want = pending ? "47" : "43";
    const char* other = pending ? "43" : "47";
    int written = 0;
    bool got = false, sawOther = false, bad = false;
    // One ECU's answer, as hex: "<mode+40> <count> <2 bytes per code>..." (zero pairs are padding).
    auto reply = [&](const char* h) {
        const char* at = anchored(h, want);
        if (!at) {
            if (anchored(h, other)) sawOther = true;
            return;
        }
        got = true;
        if (strlen(at) < 2) { bad = true; return; }
        const int count = hexVal(at[0]) << 4 | hexVal(at[1]);
        at += 2;
        if ((int)(strlen(at) / 4) < count) { bad = true; return; }   // cut off
        for (int k = 0; k < count; ++k, at += 4) {
            const int b0 = hexVal(at[0]) << 4 | hexVal(at[1]), b1 = hexVal(at[2]) << 4 | hexVal(at[3]);
            if (b0 < 0 || b1 < 0) { bad = true; return; }
            if (b0 == 0 && b1 == 0) continue;
            char code[6] = {"PCBU"[b0 >> 6], (char)('0' + ((b0 >> 4) & 3)), hexDigit((unsigned)b0),
                            hexDigit((unsigned)b1 >> 4), hexDigit((unsigned)b1), '\0'};
            bool dup = false;                                  // two ECUs can report the same code
            for (int j = 0; j < written; ++j) dup |= strcmp(out[j], code) == 0;
            if (dup || written >= max) continue;
            memcpy(out[written++], code, sizeof code);
        }
    };
    char mf[192];                                              // a multi-frame answer, joined
    size_t mfn = 0;
    mf[0] = '\0';
    for (const char* line = raw; *line;) {
        const size_t len = strcspn(line, "\r\n");
        char h[96];
        size_t n = 0;
        bool framed = false;
        if (lineHex(line, len, h, sizeof h, &n, &framed)) {
            if (framed) {
                if (mfn + n < sizeof mf) { memcpy(mf + mfn, h, n + 1); mfn += n; }
            } else {
                reply(h);
            }
        }
        line += len;
        line += strspn(line, "\r\n");
    }
    if (mfn) reply(mf);
    if (bad) return -1;
    if (got) return written;
    return !sawOther && contains(raw, "NO DATA") ? 0 : -1;
}

bool allowed(const char* cmd) {
    if (!cmd) return false;
    size_t n = strlen(cmd);
    if (n == 0 || cmd[n - 1] != '\r') return false;      // exactly one trailing '\r' (without it the
    --n;                                                 // ELM would join it to the next command)
    if (n == 4 && cmd[0] == '0' && cmd[1] == '1') {      // mode 01, one PID
        for (size_t i = 2; i < 4; ++i)
            if (!((cmd[i] >= '0' && cmd[i] <= '9') || (cmd[i] >= 'A' && cmd[i] <= 'F'))) return false;
        return true;
    }
    if (n == 2 && cmd[0] == '0' && (cmd[1] == '3' || cmd[1] == '7')) return true;   // read trouble codes
    static const char* const SETUP[] = {"ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATH1", "ATSP0", "ATRV",
                                        "ATI", "ATDPN"};
    for (const char* s : SETUP)
        if (strlen(s) == n && strncmp(cmd, s, n) == 0) return true;
    return false;
}

bool setupOk(const char* cmd, const char* raw) {
    if (!cmd || !raw) return false;
    if (strncmp(cmd, "ATZ", 3) == 0) return contains(raw, "ELM") || contains(raw, "OK");
    return contains(raw, "OK");
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
