#include "sigreq.h"
#include <cstdio>
#include <cstring>
#include "hmac256.h"

namespace sigreq {
namespace {
const char HEX[] = "0123456789abcdef";

bool reserved(const char* name) { return !strcmp(name, "t") || !strcmp(name, "n") || !strcmp(name, "sig"); }

// Appends to a fixed buffer; once it overflows, everything after is dropped and ok() stays false.
struct Out {
    char* buf;
    size_t cap, len = 0;
    bool over = false;
    void put(char c) {
        if (len + 1 >= cap) { over = true; return; }
        buf[len++] = c;
        buf[len] = 0;
    }
    void puts(const char* s) { while (*s) put(*s++); }
    void enc(const char* s) {
        static const char UP[] = "0123456789ABCDEF";
        for (; *s; ++s) {
            const unsigned char c = (unsigned char)*s;
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '.' ||
                c == '_' || c == '~') {
                put((char)c);
            } else {
                put('%');
                put(UP[c >> 4]);
                put(UP[c & 15]);
            }
        }
    }
    bool ok() const { return !over; }
};

// METHOD|path|params|t|n into msg (MAX_MSG + 1); its length, or 0 when it doesn't fit.
size_t message(const char* method, const char* path, const Param* params, int count, const char* t, const char* n,
               char* msg) {
    const Param* sorted[MAX_PARAMS];
    int m = 0;
    for (int i = 0; i < count; ++i) {
        if (!params[i].name || reserved(params[i].name)) continue;
        if (m == MAX_PARAMS) return 0;
        sorted[m++] = &params[i];
    }
    for (int i = 1; i < m; ++i)   // insertion sort: a handful of params
        for (int j = i; j > 0; --j) {
            const int c = strcmp(sorted[j - 1]->name, sorted[j]->name);
            const char* va = sorted[j - 1]->value ? sorted[j - 1]->value : "";
            const char* vb = sorted[j]->value ? sorted[j]->value : "";
            if (c < 0 || (c == 0 && strcmp(va, vb) <= 0)) break;
            const Param* tmp = sorted[j];
            sorted[j] = sorted[j - 1];
            sorted[j - 1] = tmp;
        }
    Out o{msg, MAX_MSG + 1};
    msg[0] = 0;
    o.puts(method);
    o.put('|');
    o.puts(path);
    o.put('|');
    for (int i = 0; i < m; ++i) {
        if (i) o.put('&');
        o.enc(sorted[i]->name);
        o.put('=');
        o.enc(sorted[i]->value ? sorted[i]->value : "");
    }
    o.put('|');
    o.puts(t);
    o.put('|');
    o.puts(n);
    return o.ok() ? o.len : 0;
}

void tag(const char* key, const char* msg, size_t len, uint8_t out[TAG]) {
    uint8_t full[hmac256::LEN];
    hmac256::mac(reinterpret_cast<const uint8_t*>(key), strlen(key), reinterpret_cast<const uint8_t*>(msg), len, full);
    memcpy(out, full, TAG);
}

int hexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;   // upper case isn't what sign() writes
}

// Exactly `digits` lower-case hex digits into out (digits / 2 bytes).
bool unhex(const char* s, size_t digits, uint8_t* out) {
    if (!s || strlen(s) != digits) return false;
    for (size_t i = 0; i < digits; i += 2) {
        const int hi = hexVal(s[i]), lo = hexVal(s[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i / 2] = (uint8_t)(hi << 4 | lo);
    }
    return true;
}

// 1..12 decimal digits, no sign.
bool parseTime(const char* s, int64_t* t) {
    if (!s) return false;
    const size_t len = strlen(s);
    if (len < 1 || len > 12) return false;
    int64_t v = 0;
    for (size_t i = 0; i < len; ++i) {
        if (s[i] < '0' || s[i] > '9') return false;
        v = v * 10 + (s[i] - '0');
    }
    *t = v;
    return true;
}
}  // namespace

bool sign(const char* key, const char* method, const char* path, const Param* params, int count, int64_t t,
          uint64_t nonce, char* out, size_t n) {
    if (n) out[0] = 0;
    if (!key || !key[0] || t < 0 || t > 999999999999LL || n < AUTH_LEN) return false;
    char ts[16], ns[17];
    snprintf(ts, sizeof ts, "%lld", (long long)t);
    for (int i = 0; i < 16; ++i) ns[i] = HEX[(nonce >> (60 - 4 * i)) & 15];
    ns[16] = 0;
    char msg[MAX_MSG + 1];
    const size_t len = message(method, path, params, count, ts, ns, msg);
    if (!len) return false;
    uint8_t sig[TAG];
    tag(key, msg, len, sig);
    char sh[2 * TAG + 1];
    for (size_t i = 0; i < TAG; ++i) {
        sh[2 * i] = HEX[sig[i] >> 4];
        sh[2 * i + 1] = HEX[sig[i] & 15];
    }
    sh[2 * TAG] = 0;
    snprintf(out, n, "t=%s&n=%s&sig=%s", ts, ns, sh);
    return true;
}

const char* name(Verdict v) {
    switch (v) {
    case Verdict::Ok: return "ok";
    case Verdict::Unsigned: return "unsigned";
    case Verdict::Malformed: return "malformed";
    case Verdict::NoClock: return "no clock";
    case Verdict::Stale: return "stale";
    case Verdict::BadSig: return "bad signature";
    case Verdict::Replay: return "replay";
    }
    return "?";
}

Verdict Verifier::check(const char* method, const char* path, const Param* params, int count, const char* t,
                        const char* n, const char* sig, bool synced, int64_t now) {
    if (!sig) return Verdict::Unsigned;
    int64_t when;
    uint8_t nonceBytes[8], got[TAG];
    if (!key_ || !key_[0] || !parseTime(t, &when) || !unhex(n, 16, nonceBytes) || !unhex(sig, 2 * TAG, got))
        return Verdict::Malformed;
    if (!synced) return Verdict::NoClock;
    if (!started_) {   // after a boot: nothing from before it (its ring is gone)
        started_ = true;
        floor_ = now - BOOT_SLACK_S;
    }
    if (when - now > WINDOW_S || now - when > WINDOW_S || when <= floor_) return Verdict::Stale;
    char msg[MAX_MSG + 1];
    const size_t len = message(method, path, params, count, t, n, msg);
    if (!len) return Verdict::Malformed;
    uint8_t want[TAG];
    tag(key_, msg, len, want);
    if (!hmac256::same(got, want, TAG)) return Verdict::BadSig;
    uint64_t nonce = 0;
    for (int i = 0; i < 8; ++i) nonce = nonce << 8 | nonceBytes[i];
    for (int i = 0; i < used_; ++i)
        if (nonces_[i] == nonce) return Verdict::Replay;
    if (used_ == RING && times_[next_] > floor_) floor_ = times_[next_];   // the one it lets go
    nonces_[next_] = nonce;
    times_[next_] = when;
    next_ = (next_ + 1) % RING;
    if (used_ < RING) ++used_;
    return Verdict::Ok;
}
}
