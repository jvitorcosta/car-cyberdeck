#pragma once
// Copy to src/config.h and fill in. src/config.h is git-ignored.

// Up to 3 WiFi networks tried in order. The phone hotspot (the car) goes FIRST: it gets two tries
// after a drop (util/roam) and the two decks pin fixed addresses on it (172.20.10.10 /
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

// The yellow deck (env ledpanel) reads the black deck here on the iPhone hotspot (lyrics, music
// mode); 172.20.10.10 is the S3's fixed address there. Both need the same REMOTE_TOKEN.
// (Called COMPANION_DECK before 2026-10-28; that name still works.)
#define BLACK_DECK_ADDR       "172.20.10.10"

// Speed-camera alerts (black deck): the iPhone's GPS 2 IP app serves NMEA over TCP here. The
// defaults are the phone itself on its hotspot; on the bench, point GPS_HOST at a PC running
// tools/nmea_replay.py.
// #define GPS_HOST           "172.20.10.1"
// #define GPS_PORT           11123
// Camera marks ("mark camera" said to the yellow deck) go online at once: each becomes an issue in this
// private repo, over the hotspot. MARKS_TOKEN is a fine-grained GitHub token for that repo alone,
// with Issues: read and write and nothing else. Empty: the marks stay on the deck (import-car).
// #define MARKS_REPO         "owner/radar-marks"
// #define MARKS_TOKEN        ""

// The yellow deck's two LED controllers (ELK-BLEDDM / MELK-... over Bluetooth LE): their addresses,
// lowercase. To find them, turn the phone's Bluetooth off (a controller takes one link at a time)
// and run `pio run -e blescan -t upload -t monitor`: it lists every advertiser by name and address.
// The controllers' own phone app shows them too. Switch one light off to tell inside from outside.
// The all-zero placeholders build, but the deck then logs "[leds] INSIDE: no address in config.h".
#define LED_INSIDE_MAC        "00:00:00:00:00:00"
#define LED_OUTSIDE_MAC       "00:00:00:00:00:00"

// The IR remote's four music keys on the yellow deck: each starts a playlist on the black deck's
// Spotify, from a random track with shuffle on. The 22-character id from the playlist's link
// (open.spotify.com/playlist/<id>), and the name the screen shows (">PLAYING <NAME>"). An empty id:
// that key shows ">NO PLAYLIST SET".
#define IR_PLAYLIST_1         ""
#define IR_PLAYLIST_1_NAME    ""
#define IR_PLAYLIST_2         ""
#define IR_PLAYLIST_2_NAME    ""
#define IR_PLAYLIST_3         ""
#define IR_PLAYLIST_3_NAME    ""
#define IR_PLAYLIST_4         ""
#define IR_PLAYLIST_4_NAME    ""
