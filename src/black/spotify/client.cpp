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
#include "../../util/black/playlists.h"
#include "../core/mem.h"
#include <freertos/semphr.h>

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
            // The watch's picker lists private playlists only with these (tools/spotify_auth.py's
            // SCOPES since 2026-11-02): said once, at the first login.
            static bool told = false;
            char scope[256];
            if (!told && jsonpath::str(s_body, "scope", scope, sizeof scope)) {
                told = true;
                const bool priv = strstr(scope, "playlist-read-private"), collab = strstr(scope, "playlist-read-collaborative");
                if (!priv || !collab)
                    Serial.printf("[spotify] token lacks%s%s: public playlists only, re-run tools/spotify_auth.py\n",
                                  priv ? "" : " playlist-read-private", collab ? "" : " playlist-read-collaborative");
            }
        }
    }
    https.end();
    Serial.printf("[spotify] access token %s (HTTP %d)\n", rc == 200 ? "OK" : "FAILED", rc);
    return rc;
}

static bool tokenFresh() { return s_auth[0] && (int32_t)(s_tokenExpiry - millis()) > 0; }

// Logs in (refreshToken) unless a failed login is still waiting out its back-off
// (netplan::LoginBackoff): then the last answer again, with no request. s_loginFail: the request
// that called it ends at the login, for the status screen.
static netplan::LoginBackoff s_login;
static bool s_loginFail = false;
static int login() {
    if (!s_login.due(millis())) { s_loginFail = true; return s_login.lastCode(); }
    const int rc = refreshToken();
    const uint32_t was = s_login.waitMs();
    s_login.onResult(rc, millis());
    s_loginFail = rc != 200;
    if (s_login.waitMs() != was && s_login.waitMs())
        Serial.printf("[spotify] login %s (HTTP %d): next try in %lus\n",
                      s_login.refused() ? "refused, re-run tools/spotify_auth.py" : "failed", rc,
                      (unsigned long)(s_login.waitMs() / 1000));
    return rc;
}

// METHOD API + path with the access token, on the kept-open connection (body: a PUT's JSON, else
// nullptr). Returns the HTTP status (< 0: no answer; a failed token refresh answers with its own
// status). A 200's body is in s_body, and a PUT's error body too (the reason Spotify gives); a
// 429's Retry-After (seconds) in *retryAfterS, else -1.
static int apiCall(const char* method, const char* path, const char* body, int* retryAfterS) {
    if (retryAfterS) *retryAfterS = -1;
    s_loginFail = false;
    if (!s_body) return -1;
    s_body[0] = '\0';
    if (!tokenFresh()) {
        int rc = login();
        if (rc != 200) return rc;
    }
    char url[256];
    snprintf(url, sizeof url, "%s%s", API, path);
    static const char* KEYS[] = {"Retry-After"};
    const bool get = strcmp(method, "GET") == 0;
    for (int attempt = 0; attempt < 2; ++attempt) {
        const bool reused = s_api.connected();
        if (!s_http.begin(s_api, url)) return -1;
        s_http.addHeader("Authorization", s_auth);
        if (body) s_http.addHeader("Content-Type", "application/json");
        s_http.collectHeaders(KEYS, 1);
        watchdog::feed();
        int rc = get ? s_http.GET() : s_http.sendRequest(method, body ? body : "");
        if (rc < 0 && reused && attempt == 0) {      // the kept connection had gone stale: once more
            s_http.end();
            s_api.stop();
            continue;
        }
        if (rc == 401 && attempt == 0) {             // token revoked or expired early
            s_http.end();
            s_api.stop();
            const int t = login();
            if (t != 200) return t;
            continue;
        }
        if (rc < 0) netcfg::certRejected(s_api, "api.spotify.com");
        bool keep = rc == 200 || rc == 204;          // bodies we read to the end (or none)
        if (rc == 200 && !readBody(s_http)) { rc = -1; keep = false; }
        if (!get && rc >= 400 && s_http.getSize() != 0) keep = readBody(s_http);   // small: the reason
        else if (!get && (rc == 201 || rc == 202)) keep = s_http.getSize() == 0 || readBody(s_http);
        if (rc == 429 && retryAfterS) *retryAfterS = netplan::parseRetryAfter(s_http.header("Retry-After").c_str());
        s_http.end();
        if (!keep) s_api.stop();                     // unread error bodies would poison the next answer
        return rc;
    }
    return -1;
}

static int apiGet(const char* path, int* retryAfterS = nullptr) { return apiCall("GET", path, nullptr, retryAfterS); }

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
    login();
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
    st.loginFailed = s_loginFail && s_login.refused();   // Spotify said no to the refresh token
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

// The watch's playlist picker. The remote task leaves one request here and waits; the network task
// runs it between its own requests (serveWatch), on the kept-open connection, so it costs no TLS
// handshake of its own. A request the network task hasn't started when the wait ends is withdrawn;
// one it has started finishes, and its answer is dropped (seq tells answers apart).
enum class Job : uint8_t { None, Page, Play };
constexpr uint32_t WATCH_WAIT_MS = 15000;   // a poll and one per-track step (lyrics: up to ~8 s) at worst
static portMUX_TYPE s_jobMux = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t s_jobDone = nullptr;
static Job s_job = Job::None;               // waiting for the network task
static bool s_jobRunning = false;
static uint32_t s_jobSeq = 0, s_doneSeq = 0;
static int s_jobOffset = 0;
static bool s_jobShuffle = false;
static char s_jobId[playlists::ID_LEN + 1];
static char s_reply[playlists::REPLY_MAX];
static int s_replyCode = 0;

static int watchRequest(Job job, int offset, const char* id, bool shuffle, char* out, size_t n) {
    if (n) out[0] = '\0';
    if (!hasToken()) { txt::copy(out, "No Spotify login", n); return 502; }
    if (!s_jobDone) s_jobDone = xSemaphoreCreateBinary();   // this task only; serveWatch needs a job first
    portENTER_CRITICAL(&s_jobMux);
    const bool busy = !s_jobDone || s_job != Job::None || s_jobRunning;
    uint32_t seq = 0;
    if (!busy) {
        s_job = job;
        s_jobOffset = offset;
        s_jobShuffle = shuffle;
        txt::copy(s_jobId, id ? id : "", sizeof s_jobId);
        seq = ++s_jobSeq;
    }
    portEXIT_CRITICAL(&s_jobMux);
    if (busy) { txt::copy(out, "busy", n); return 503; }
    const uint32_t start = millis();
    for (;;) {
        const uint32_t spent = millis() - start;
        if (spent >= WATCH_WAIT_MS) break;
        xSemaphoreTake(s_jobDone, pdMS_TO_TICKS(WATCH_WAIT_MS - spent));   // a dropped answer's signal too
        portENTER_CRITICAL(&s_jobMux);
        const bool done = s_doneSeq == seq;
        portEXIT_CRITICAL(&s_jobMux);
        if (done) {   // nothing writes s_reply until the next request, which this task makes
            txt::copy(out, s_reply, n);
            return s_replyCode;
        }
    }
    portENTER_CRITICAL(&s_jobMux);
    if (s_job != Job::None && s_jobSeq == seq) s_job = Job::None;   // not started: withdrawn
    portEXIT_CRITICAL(&s_jobMux);
    txt::copy(out, "Spotify slow: retry", n);
    return 504;
}

int watchPlaylists(int offset, char* out, size_t n) { return watchRequest(Job::Page, offset, nullptr, false, out, n); }
int watchPlay(const char* id, bool shuffle, char* out, size_t n) { return watchRequest(Job::Play, 0, id, shuffle, out, n); }

static void answer(int code, const char* text) {
    s_replyCode = code;
    txt::copy(s_reply, text, sizeof s_reply);
}

// One page of /v1/me/playlists into s_reply. A page too big for the answer buffer (long
// descriptions) is asked again at half the size.
static void runPage(int offset) {
    for (int limit = playlists::PAGE; limit >= playlists::PAGE / 2; limit /= 2) {
        char path[64];
        snprintf(path, sizeof path, "/v1/me/playlists?limit=%d&offset=%d", limit, offset);
        const int rc = apiGet(path);
        if (rc == -1 && s_sink.overflow) continue;
        if (rc == 200 && playlists::writePage(s_body, offset, s_reply, sizeof s_reply) >= 0) {
            s_replyCode = 200;
            return;
        }
        Serial.printf("[spotify] playlists offset %d: HTTP %d\n", offset, rc);
        answer(502, rc == 200 ? "Spotify error" : playlists::why(rc, "", false));   // 200: not a page
        return;
    }
    Serial.printf("[spotify] playlists offset %d: page too big\n", offset);
    answer(502, "Spotify error");
}

// shuffle: from a random track (the playlist's total asked first; without it, the first track),
// then Spotify's shuffle on, so what follows is shuffled too. A shuffle that fails is only logged:
// the music plays.
static void runPlay(const char* id, bool shuffle) {
    int position = -1;
    if (shuffle && playlists::validId(id)) {
        char path[80];
        snprintf(path, sizeof path, "/v1/playlists/%s?fields=items.total", id);
        int total = 0;
        const int rc = apiGet(path);
        if (rc == 200 && playlists::readTotal(s_body, &total)) position = playlists::startPosition(total, esp_random());
        else Serial.printf("[spotify] playlist %s total: HTTP %d\n", id, rc);
    }
    char body[96];
    if (!playlists::playBodyAt(id, position, body, sizeof body)) { answer(400, "bad id"); return; }
    int rc = apiCall("PUT", "/v1/me/player/play", body, nullptr);
    if (rc == 404) {   // no active device: Spotify open but idle; play on one it lists (a phone first)
        char device[64];
        const int dr = apiGet("/v1/me/player/devices");
        if (dr == 200 && playlists::pickDevice(s_body, device, sizeof device)) {
            char path[112];
            snprintf(path, sizeof path, "/v1/me/player/play?device_id=%s", device);
            rc = apiCall("PUT", path, body, nullptr);
            Serial.printf("[spotify] no active device: asked device %s: HTTP %d\n", device, rc);
        } else {
            Serial.printf("[spotify] no active device, none listed (HTTP %d)\n", dr);
            rc = 404;
        }
    }
    if (shuffle) Serial.printf("[spotify] play playlist %s shuffled at %d: HTTP %d\n", id, position, rc);
    else Serial.printf("[spotify] play playlist %s: HTTP %d\n", id, rc);
    if (rc != 200 && rc != 202 && rc != 204) { answer(502, playlists::why(rc, s_body, true)); return; }
    if (shuffle) {
        const int sh = apiCall("PUT", "/v1/me/player/shuffle?state=true", nullptr, nullptr);
        if (sh != 200 && sh != 202 && sh != 204) Serial.printf("[spotify] shuffle on: HTTP %d\n", sh);
    }
    answer(200, "playing");
}

void serveWatch(bool rateLimited) {
    portENTER_CRITICAL(&s_jobMux);
    const Job job = s_job;
    const uint32_t seq = s_jobSeq;
    const int offset = s_jobOffset;
    const bool shuffle = s_jobShuffle;
    char id[sizeof s_jobId];
    memcpy(id, s_jobId, sizeof id);
    if (job != Job::None) { s_job = Job::None; s_jobRunning = true; }
    portEXIT_CRITICAL(&s_jobMux);
    if (job == Job::None) return;
    if (rateLimited) {
        answer(502, playlists::why(429, "", false));   // asking now would only stretch the wait
    } else if (!s_api.connected() && mem::byteLargest() < netplan::TLS_NEED) {
        answer(503, "low memory");   // a handshake now could starve the next poll's
    } else if (job == Job::Page) {
        runPage(offset);
    } else {
        runPlay(id, shuffle);
    }
    portENTER_CRITICAL(&s_jobMux);
    s_doneSeq = seq;
    s_jobRunning = false;
    portEXIT_CRITICAL(&s_jobMux);
    xSemaphoreGive(s_jobDone);
}

}
