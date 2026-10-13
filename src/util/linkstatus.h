#pragma once
#include <cstddef>
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
// saved networks ("Cecilia, iPhone") for NoWifi. Cut texts end in "...".
void describe(Kind k, int lastCode, const char* network, char* title, size_t titleN,
              char* detail, size_t detailN);
}
