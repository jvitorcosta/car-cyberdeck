#include "linkstatus.h"
#include <cstdio>
#include <cstring>

namespace linkstatus {

Kind classify(const Input& in) {
    if (!in.wifiUp) return Kind::NoWifi;
    if (!in.down) return Kind::Ok;
    if (in.certRejected) return Kind::CertRejected;
    // An HTTP answer other than success means Spotify itself is talking back.
    if (in.lastCode > 0 && in.lastCode != 200 && in.lastCode != 204) return Kind::SpotifyError;
    return Kind::NoInternet;
}

// Copies src into dst (n bytes) and at most `max` characters; a cut text ends in "...".
static void fit(char* dst, size_t n, const char* src, size_t max) {
    if (n == 0) return;
    size_t cap = n - 1 < max ? n - 1 : max;
    size_t len = strlen(src);
    if (len <= cap) { memcpy(dst, src, len + 1); return; }
    size_t keep = cap >= 3 ? cap - 3 : cap;
    while (keep > 0 && ((unsigned char)src[keep] & 0xC0) == 0x80) --keep;   // whole UTF-8 chars
    memcpy(dst, src, keep);
    if (cap >= 3) { memcpy(dst + keep, "...", 3); keep += 3; }
    dst[keep] = '\0';
}

void describe(Kind k, int lastCode, const char* network, char* title, size_t titleN,
              char* detail, size_t detailN) {
    char d[160];
    const char* t = "";
    const char* net = network && network[0] ? network : "WiFi";
    switch (k) {
    case Kind::NoWifi:       t = "No WiFi...";     snprintf(d, sizeof d, "Looking for %s", net); break;
    case Kind::NoInternet:   t = "No internet..."; snprintf(d, sizeof d, "%s has no internet", net); break;
    case Kind::SpotifyError: t = "Spotify error";  snprintf(d, sizeof d, "HTTP %d, retrying...", lastCode); break;
    case Kind::CertRejected: t = "Spotify error";  snprintf(d, sizeof d, "Certificate rejected"); break;
    case Kind::Ok:           t = "";               d[0] = '\0'; break;
    }
    fit(title, titleN, t, TITLE_MAX);
    fit(detail, detailN, d, DETAIL_MAX);
}

}
