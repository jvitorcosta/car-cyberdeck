#pragma once
// Copy to src/config.h and fill in. src/config.h is git-ignored.

// Up to 3 WiFi networks tried in order. The phone hotspot (the car) goes FIRST: it gets two tries
// after a drop (util/roam) and the deck and the companion pin fixed addresses on it (172.20.10.10 /
// .11, iPhone only). Enable "Maximize Compatibility" on the iPhone: the ESP32 only speaks 2.4 GHz.
struct WifiCred { const char* ssid; const char* pass; };
static const WifiCred WIFI_NETWORKS[] = {
    {"YOUR_IPHONE_HOTSPOT", "YOUR_HOTSPOT_PASSWORD"},
    {"YOUR_HOME_SSID", "YOUR_HOME_PASSWORD"},
};
static const int WIFI_NETWORK_COUNT = 2;

// Spotify developer app (https://developer.spotify.com/dashboard)
#define SPOTIFY_CLIENT_ID     "your_client_id"
#define SPOTIFY_CLIENT_SECRET "your_client_secret"
// Spotify requires http only for the loopback address. Register this EXACT URI in
// your app dashboard (Settings -> Redirect URIs). Used by the PC auth helper.
#define SPOTIFY_REDIRECT_URI  "http://127.0.0.1:8888/callback"
// One-time refresh token, obtained on the PC via `python tools/spotify_auth.py`.
#define SPOTIFY_REFRESH_TOKEN ""
#define SPOTIFY_MARKET        "BR"

// Your time zone as a POSIX TZ string (night mode runs 18:00-06:00 local time). Examples:
//   "<-04>4" Manaus, "<-03>3" Sao Paulo, "WET0WEST,M3.5.0/1,M10.5.0" Lisbon,
//   "CET-1CEST,M3.5.0,M10.5.0/3" Berlin, "EST5EDT,M3.2.0,M11.1.0" New York, "UTC0" UTC.
// More: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
#define TIMEZONE              "UTC0"

// Phone remote: open http://cardeck.local/?k=<this token> on a phone on the same network
// (uncomment REMOTE_NAME below to rename).
// Any long random string; empty turns the remote off. The deck logs its address at boot.
#define REMOTE_TOKEN          ""
// #define REMOTE_NAME        "cardeck"

// Firmware updates over WiFi (both decks): a different random string, never REMOTE_TOKEN; only
// used by `pio run -e *_ota -t upload` (tools/ota_auth.py reads it from here). Empty turns them off.
#define OTA_PASSWORD          ""

// Companion screen (env cyd_companion): the new deck it reads. 172.20.10.10 is the S3's fixed
// address on the iPhone hotspot. Both need the same REMOTE_TOKEN.
#define COMPANION_DECK        "172.20.10.10"
