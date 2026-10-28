#include "client.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_heap_caps.h>
#include "../../common/net/http_config.h"
#include "../../common/core/watchdog.h"
#include "../../config.h"
#include "../../util/black/ctxcache.h"
#include "../../util/common/jsonpath.h"
#include "../../util/black/playerjson.h"
#include "../../util/common/text.h"
#include "../../util/black/netplan.h"

// Spotify Web API client: one request per poll (GET /v1/me/player: the song, its progress and the
// device) on a connection kept open between polls, so a poll costs a request instead of a TLS
// handshake (~1.5 s and ~40 KB of heap churn each). Replaces SpotifyArduino, which opened a new
// connection per call, kept a second access token, dropped the response headers (Retry-After
// cost another request) and left fields uninitialised for ads.
namespace spclient {

constexpr const char* API = "https://api.spotify.com";
// market: relinked songs and no available_markets lists (they were most of the body);
// additional_types: podcasts as items, not nulls.
constexpr const char* PLAYER_PATH = "/v1/me/player?market=" SPOTIFY_MARKET "&additional_types=episode";
constexpr size_t BODY_MAX = 16 * 1024;   // a /me/player answer is ~4 KB
constexpr uint32_t TOKEN_MARGIN_S = 300; // refresh this long before Spotify's expiry

static WiFiClientSecure s_api;           // api.spotify.com, kept open between requests
static HTTPClient s_http;
// "Bearer xxx" (~250 chars today). A fixed buffer, not a heap String: the String's block (~270 B,
// kept for the whole session) landed between two ~17.7 KB free blocks and capped the largest free
// block at ~17-22 KB instead of ~35 KB, below TLS_NEED (heap dump, 2026-10-08).
static char s_auth[512] = "";
static uint32_t s_tokenExpiry = 0;
static char* s_body = nullptr;           // the last answer's body, NUL-terminated (PSRAM)
static size_t s_bodyLen = 0;
// The current context's playlist description (music mode's mood, util/songmood): fetched with its
// name in one request and cached with it (ctxcache).
static char s_desc[160] = "";
static bool s_fromPlaylist = false;   // as of the last poll with a song

// HTTPClient::writeToStream target: copies the body into s_body (chunked or not), and keeps
// accepting bytes past BODY_MAX (dropped, flagged) so the connection is read to its end and
// stays usable.
class BodySink : public Stream {
public:
    bool overflow = false;
    void reset() { s_bodyLen = 0; overflow = false; if (s_body) s_body[0] = '\0'; }
    size_t write(uint8_t c) override { return write(&c, 1); }
    size_t write(const uint8_t* b, size_t n) override {
        if (!s_body) return n;
        size_t room = BODY_MAX - 1 - s_bodyLen;
        size_t k = n < room ? n : room;
        if (k < n) overflow = true;
        memcpy(s_body + s_bodyLen, b, k);
        s_bodyLen += k;
        s_body[s_bodyLen] = '\0';
        return n;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
};
static BodySink s_sink;

// Reads a 200's body into s_body. False when it was cut off or too big.
static bool readBody(HTTPClient& h) {
    s_sink.reset();
    if (!s_body) return false;
    const int n = h.writeToStream(&s_sink);
    if (n < 0 || s_sink.overflow) {
        Serial.printf("[spotify] body %s (%d)\n", s_sink.overflow ? "too big" : "cut off", n);
        return false;
    }
    return true;
}

bool hasToken() { return SPOTIFY_REFRESH_TOKEN[0] != '\0'; }

// A new access token from the refresh token (tools/spotify_auth.py). Returns the HTTP status of
// accounts.spotify.com (200 = token stored; < 0 = no answer).
static int refreshToken() {
    WiFiClientSecure c;
    netcfg::spotify(c);
    HTTPClient https;
    https.setTimeout(netcfg::HTTP_TIMEOUT_MS);
    https.setReuse(false);
    if (!https.begin(c, "https://accounts.spotify.com/api/token")) return -1;
    https.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String body = "grant_type=refresh_token&refresh_token=" + String(SPOTIFY_REFRESH_TOKEN) +
                  "&client_id=" + SPOTIFY_CLIENT_ID +
                  "&client_secret=" + SPOTIFY_CLIENT_SECRET;
    watchdog::feed();   // each request is bounded by its own timeouts (common/core/watchdog)
    int rc = https.POST(body);
    if (rc < 0) netcfg::certRejected(c, "accounts.spotify.com");
    if (rc == 200) {
        char token[400];
        long expiresS = 3600;
        if (!readBody(https) || !jsonpath::str(s_body, "access_token", token, sizeof token)) {
            rc = -1;
        } else if (!txt::concat(s_auth, sizeof(s_auth), "Bearer ", token)) {
            Serial.println("[spotify] access token too long for its buffer");
            rc = -1;
        } else {
            jsonpath::num(s_body, "expires_in", &expiresS);
            if (expiresS < 2 * (long)TOKEN_MARGIN_S) expiresS = 2 * TOKEN_MARGIN_S;
            s_tokenExpiry = millis() + (uint32_t)(expiresS - TOKEN_MARGIN_S) * 1000;
        }
    }
    https.end();
    Serial.printf("[spotify] access token %s (HTTP %d)\n", rc == 200 ? "OK" : "FAILED", rc);
    return rc;
}

static bool tokenFresh() { return s_auth[0] && (int32_t)(s_tokenExpiry - millis()) > 0; }

// GET API + path with the access token, on the kept-open connection. Returns the HTTP status
// (< 0: no answer; a failed token refresh answers with its own status). A 200's body is in
// s_body; a 429's Retry-After (seconds) in *retryAfterS, else -1.
static int apiGet(const char* path, int* retryAfterS = nullptr) {
    if (retryAfterS) *retryAfterS = -1;
    if (!s_body) return -1;
    if (!tokenFresh()) {
        int rc = refreshToken();
        if (rc != 200) return rc;
    }
    char url[256];
    snprintf(url, sizeof url, "%s%s", API, path);
    static const char* KEYS[] = {"Retry-After"};
    for (int attempt = 0; attempt < 2; ++attempt) {
        const bool reused = s_api.connected();
        if (!s_http.begin(s_api, url)) return -1;
        s_http.addHeader("Authorization", s_auth);
        s_http.collectHeaders(KEYS, 1);
        watchdog::feed();
        int rc = s_http.GET();
        if (rc < 0 && reused && attempt == 0) {      // the kept connection had gone stale: once more
            s_http.end();
            s_api.stop();
            continue;
        }
        if (rc == 401 && attempt == 0) {             // token revoked or expired early
            s_http.end();
            s_api.stop();
            const int t = refreshToken();
            if (t != 200) return t;
            continue;
        }
        if (rc < 0) netcfg::certRejected(s_api, "api.spotify.com");
        bool keep = rc == 200 || rc == 204;          // bodies we read to the end (or none)
        if (rc == 200 && !readBody(s_http)) { rc = -1; keep = false; }
        if (rc == 429 && retryAfterS) *retryAfterS = netplan::parseRetryAfter(s_http.header("Retry-After").c_str());
        s_http.end();
        if (!keep) s_api.stop();                     // unread error bodies would poison the next answer
        return rc;
    }
    return -1;
}

void begin() {
    netcfg::spotify(s_api);
    s_api.setTimeout(8);               // seconds: TCP connect/read (default 30 s)
    s_http.setReuse(true);
    s_http.setTimeout(netcfg::HTTP_TIMEOUT_MS);
    s_http.setUserAgent(netcfg::USER_AGENT);
    if (!s_body) {
        s_body = (char*)heap_caps_malloc(BODY_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_body) s_body = (char*)malloc(BODY_MAX);
        if (s_body) s_body[0] = '\0';
        else Serial.println("[spotify] no memory for the answer buffer");
    }
    refreshToken();
}

// uri like "spotify:playlist:ID" / ":album:" / ":artist:". Returns false when the lookup
// failed for a reason worth retrying (no token, network, 5xx/429); `out` then holds the type
// ("playlist") as a placeholder. 403/404 (private or deleted playlist) are final. A playlist's
// description comes in the same request, into desc (dn bytes, HTML stripped; "" otherwise).
static bool resolveContext(const char* uri, char* out, size_t n, char* desc, size_t dn) {
    out[0] = '\0';
    desc[0] = '\0';
    const char* p1 = uri ? strchr(uri, ':') : nullptr;
    const char* p2 = p1 ? strchr(p1 + 1, ':') : nullptr;
    if (!p2) { txt::copy(out, "-", n); return true; }
    char type[16];
    txt::copy(type, p1 + 1, (size_t)(p2 - p1) < sizeof type ? (size_t)(p2 - p1) : sizeof type);
    const char* endpoint = strcmp(type, "playlist") == 0 ? "playlists"
                         : strcmp(type, "album") == 0    ? "albums"
                         : strcmp(type, "artist") == 0   ? "artists" : nullptr;
    txt::copy(out, type, n);                   // placeholder until resolved
    if (!endpoint) return true;                // e.g. "collection": nothing to look up
    const bool playlist = strcmp(endpoint, "playlists") == 0;
    char path[160];
    snprintf(path, sizeof path, "/v1/%s/%s?fields=%s", endpoint, p2 + 1, playlist ? "name,description" : "name");
    const int rc = apiGet(path);
    if (rc == 200 && jsonpath::str(s_body, "name", out, n)) {   // raw; folded at display time
        if (playlist && jsonpath::str(s_body, "description", desc, dn)) txt::stripHtml(desc);   // music mode's mood
        return true;
    }
    if (rc == 200) txt::copy(out, type, n);
    return rc == 403 || rc == 404;
}

bool fromPlaylist() { return s_fromPlaylist; }
const char* playlistDescription() { return s_fromPlaylist ? s_desc : ""; }

static void copyStr(char* dst, const char* src, size_t n) { txt::copy(dst, src, n); }

void poll(AppState& st) {
    if (!s_body) return;
    int retryAfter = -1;
    const int code = apiGet(PLAYER_PATH, &retryAfter);
    st.lastPollCode = code;
    st.certRejected = false;
    st.retryAfterS = code == 429 ? retryAfter : -1;
    if (code == 204) {                          // no active device: nothing to show
        if (st.status != PlaybackStatus::Stopped) Serial.println("[spotify] poll 204: no active device");
        st.status = PlaybackStatus::Stopped;
        return;
    }
    static playerjson::Now n;                   // ~900 B: off the network task's 10 KB stack
    if (code == 200 && !playerjson::parse(s_body, n)) {
        Serial.printf("[spotify] poll 200 but not a player answer (%u B)\n", (unsigned)s_bodyLen);
        st.lastPollCode = -1;
    }
    if (st.lastPollCode != 200) {
        if (code < 0) st.certRejected = netcfg::certRejected(s_api, "api.spotify.com");
        if (!st.certRejected) Serial.printf("[spotify] poll HTTP %d\n", st.lastPollCode);
        st.status = PlaybackStatus::Offline;
        return;
    }
    copyStr(st.deviceName, n.deviceName, sizeof(st.deviceName));
    copyStr(st.deviceType, n.deviceType, sizeof(st.deviceType));
    if (n.volume >= 0) st.volume = n.volume;
    st.shuffle = n.shuffle;
    st.repeat = n.repeat;
    st.isPlaying = n.playing;
    st.lastPollMs = millis();
    if (n.kind != playerjson::Kind::Track && n.kind != playerjson::Kind::Episode) {
        // An ad (free accounts), an unknown item or none: "Waiting for a song..." until music
        // resumes. The last song's fields stay as they were, so no new track is detected.
        static uint32_t lastLog = 0;
        if (millis() - lastLog > 30000) {
            lastLog = millis();
            Serial.printf("[spotify] poll 200 but no song (%s, playing %d)\n",
                          n.kind == playerjson::Kind::Other ? "ad or unknown item" : "no item", (int)n.playing);
        }
        st.status = PlaybackStatus::Stopped;
        return;
    }
    // Store ORIGINAL (UTF-8) names so LRCLIB lyric matching works for accented
    // titles; accents are folded to ASCII at display time instead.
    copyStr(st.trackName, n.name, sizeof(st.trackName));
    txt::copyId(n.uri, st.trackUri, sizeof(st.trackUri));   // long local-file URIs stay distinct
    copyStr(st.artist, n.artist, sizeof(st.artist));
    copyStr(st.album, n.album, sizeof(st.album));
    copyStr(st.albumArtUrl, n.artUrl, sizeof(st.albumArtUrl));
    st.progressMs = n.progressMs;
    st.durationMs = n.durationMs;
    st.status = n.playing ? PlaybackStatus::Playing : PlaybackStatus::Paused;

    // The context (playlist/album/artist) name: resolved once per context change (cached), on
    // the same connection. A failed lookup is retried after ctxcache::RETRY_MS.
    static ctxcache::Cache cache;
    static char cachedName[64] = "";
    s_fromPlaylist = strncmp(n.context, "spotify:playlist:", 17) == 0;
    if (ctxcache::fromAlbum(n.context)) {
        // from an album, or no context (a song from search): the song's own album name,
        // no lookup (the album endpoint's whole track list was too big to parse)
        txt::copy(st.context, st.album, sizeof(st.context));
    } else {
        if (cache.needsLookup(n.context, millis())) {
            bool ok = resolveContext(n.context, cachedName, sizeof(cachedName), s_desc, sizeof(s_desc));
            cache.store(n.context, ok, millis());
            Serial.printf("[spotify] context: %s%s\n", cachedName, ok ? "" : " (lookup failed, will retry)");
        }
        txt::copy(st.context, cachedName, sizeof(st.context));
    }
}

}
