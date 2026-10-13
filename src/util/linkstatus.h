#pragma once
#include <cstddef>
#include <cstdint>
// Which status screen to show when Spotify can't be polled, and its text. "No signal" used
// to cover all of these; on the road the useful question is whose fault it is: the WiFi
// (turn the hotspot on), the hotspot's internet (mobile data, quota) or Spotify (wait).
// Pure: main.cpp feeds the state, ui::drawOffline draws the result.
namespace linkstatus {
enum class Kind { Ok, NoWifi, NoInternet, SpotifyError, CertRejected };

struct Input {
    bool wifiUp;
    bool down;           // debounced: failed polls in a row (netplan::LinkGate) or stale
    int lastCode;        // last poll's HTTP status; < 0 = no answer (connect/TLS/timeout)
    bool certRejected;   // last failure was a rejected certificate (netcfg::certRejected)
};
Kind classify(const Input& in);

constexpr size_t TITLE_MAX = 15;    // font 4 in the 280 px dialogue box
constexpr size_t DETAIL_MAX = 32;   // font 2 on the line under it (wide letters included)
// Title and detail for a screen. `network` is the connected network for NoInternet and the
// saved networks ("Cecilia, iPhone") for NoWifi. retryS: seconds to the next poll (shown as a
// countdown while Spotify rate-limits, HTTP 429), < 0 = unknown. Cut texts end in "...".
void describe(Kind k, int lastCode, int retryS, const char* network, char* title, size_t titleN,
              char* detail, size_t detailN);
// Whole seconds (rounded up) from nowMs to nextPollMs, 0 when overdue, -1 when nextPollMs is 0
// (nothing planned). millis() wrap-safe.
int retryInS(uint32_t nowMs, uint32_t nextPollMs);
}
