#pragma once
#include <cstddef>
#include <cstdint>
// Signed HTTP requests between the decks and the watch, so no device sends the bearer key: whoever
// holds a deck's address (a passenger's phone that got 172.20.10.10 by DHCP) learns nothing it can
// reuse. PURE, host-tested (test_sigreq). A signed request carries, besides its own params,
//   t=<unix s>&n=<16 hex nonce>&sig=<32 hex>
//   sig = the first 16 bytes of HMAC-SHA256(REMOTE_TOKEN, METHOD|path|params|t|n)
// params: the request's other params (all but t, n and sig), sorted by name (then value), each
// name=value percent-encoded (all but A-Z a-z 0-9 - . _ ~ as %XX, upper-case hex) and joined by &;
// the values as the server decodes them. So the order the client sent them in, and how it encoded
// them (+ or %20), don't matter; and no value can pose as another param.
// A server accepts it when |t - now| <= WINDOW_S and the nonce isn't in its ring of the last RING
// accepted ones. Requests that fell out of the ring are refused by time: t must be above the
// newest t that left it (and, after a boot, at or after the boot instant: now - uptime at the first
// check), so the window stays bounded whatever the request rate. Anything that goes wrong computing
// the HMAC refuses the request (and sign() sends nothing): it never fails open.
namespace sigreq {
constexpr int64_t WINDOW_S = 120;
constexpr int RING = 32;
constexpr size_t TAG = 16;          // bytes of the HMAC sent
constexpr size_t MAX_MSG = 384;     // the signed string; longer requests aren't signed or accepted
constexpr int MAX_PARAMS = 8;
// What sign() adds to a query: "t=" 12 digits at most, "&n=" 16, "&sig=" 32.
constexpr size_t AUTH_LEN = 2 + 12 + 3 + 16 + 5 + 32 + 1;

struct Param {
    const char* name;
    const char* value;
};

// Writes "t=<t>&n=<nonce>&sig=<hex>" into out. False (out "") when out is short, the key is empty,
// there are more than MAX_PARAMS params, or the signed string would pass MAX_MSG.
bool sign(const char* key, const char* method, const char* path, const Param* params, int count, int64_t t,
          uint64_t nonce, char* out, size_t n);

enum class Verdict {
    Ok,
    Unsigned,   // no sig param: the caller may still accept a bearer key
    Malformed,  // t, n or sig missing or not in their format, too many params, too long
    NoClock,    // this side's clock isn't set yet: nothing signed can be checked
    Stale,      // t outside the window, or below what the ring has already let go
    BadSig,
    Replay,     // a nonce the ring has seen
    Failed,     // the HMAC couldn't be computed: refused
};
const char* name(Verdict v);

class Verifier {
public:
    explicit Verifier(const char* key) : key_(key) {}
    // params: the request's params; t, n and sig among them are skipped (their values come apart,
    // null when absent). synced: now is a real UTC time. uptimeS: seconds since this side booted
    // (its ring is empty since then: nothing signed before the boot instant is accepted). Only an
    // Ok is remembered in the ring.
    Verdict check(const char* method, const char* path, const Param* params, int count, const char* t,
                  const char* n, const char* sig, bool synced, int64_t now, int64_t uptimeS);

private:
    const char* key_;
    uint64_t nonces_[RING] = {};
    int64_t times_[RING] = {};
    int used_ = 0, next_ = 0;
    bool started_ = false;
    int64_t floor_ = 0;   // only t > floor_ is accepted
};
}
