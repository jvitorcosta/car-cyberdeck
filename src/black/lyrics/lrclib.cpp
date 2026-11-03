#include "lrclib.h"
#include "../../util/black/lrcmatch.h"
#include "../../util/black/lrcstream.h"
#include "../../util/black/netplan.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "../../common/net/http_config.h"
#include "../../common/core/watchdog.h"
#include "../../util/common/text.h"
#include "../../util/common/readguard.h"
#include "../core/mem.h"

namespace lyricsvc {

static constexpr uint32_t LYRICS_DEADLINE_MS = 15000;   // whole body, per request
// ~5.6 KB in PSRAM (only the CPU touches it), allocated on first use: the network task's first
// lyrics step. A failed allocation is tried again on the next call.
lyricbuf::Lyrics* arena() {
    static lyricbuf::Lyrics* a = nullptr;
    if (!a) {
        auto* p = (lyricbuf::Lyrics*)mem::bigAlloc(sizeof(lyricbuf::Lyrics));
        if (p) lyricbuf::reset(*p);
        a = p;
    }
    return a;
}

// Query value via the shared, bounded encoder (one String instead of one per byte). AppState
// fields are < 96 bytes, so the encoded form (<= 3 bytes each) always fits.
static String urlEncode(const char* s) {
    char buf[300];
    txt::urlEncode(s, buf, sizeof(buf));
    return String(buf);
}

// One request into `out`. *rc: HTTP status (negative: HTTPClient connect/TLS/timeout error).
// *stalled: the body stopped arriving before the reader was done. title (search): the song the
// entries must be. keepPlain: out holds /api/get's plain lines; they stay unless synced lines come,
// and the kind reports them whatever this request's fate.
static lrcstream::Kind request(const String& url, lrcstream::Mode mode, uint32_t durationMs,
                               const char* title, bool keepPlain,
                               lyricbuf::Lyrics& out, int* rc, bool* stalled) {
    *stalled = false;
    const lrcstream::Kind none = keepPlain ? lrcstream::Kind::Plain : lrcstream::Kind::None;
    if (!keepPlain) lyricbuf::reset(out);
    if (!netcfg::linkUp()) { *rc = -1; return none; }
    WiFiClientSecure client;
    netcfg::secure(client);
    HTTPClient https;
    netcfg::streamed(https);   // plain (non-chunked) body: scan straight from the stream
    if (!https.begin(client, url)) { *rc = -1; return none; }
    https.addHeader("User-Agent", netcfg::USER_AGENT);
    watchdog::feed();   // each request is bounded by its own timeouts (common/core/watchdog)
    *rc = https.GET();
    if (*rc != 200) { https.end(); return none; }
    // Stream straight into the arena (no JSON document: ArduinoJson grew the string by
    // doubling, up to 16 KB contiguous while TLS was open).
    lrcstream::Extractor ex(out, mode, durationMs, title, keepPlain);
    WiFiClient* s = https.getStreamPtr();
    uint8_t chunk[128];
    // Stall: the body stopped. Deadline: a trickling server would otherwise hold the network
    // task (and the Spotify polls) for as long as it keeps sending a byte every few seconds.
    readguard::Guard guard(millis(), netcfg::STALL_MS, LYRICS_DEADLINE_MS);
    while (!ex.done() && (https.connected() || s->available())) {
        int a = s->available();
        if (a <= 0) {
            if (guard.check(millis()) != readguard::State::Ok) { *stalled = true; break; }
            delay(1);
            continue;
        }
        int r = s->read(chunk, a < (int)sizeof(chunk) ? a : (int)sizeof(chunk));
        for (int i = 0; i < r && !ex.done(); ++i) ex.feed((char)chunk[i]);
        guard.onData(millis());
        if (guard.check(millis()) == readguard::State::Deadline) { *stalled = true; break; }
    }
    https.end();
    ex.finish();
    if (!ex.sawJson()) *stalled = true;   // 200 with an HTML page (captive portal): retry later
    return ex.kind();
}

lyricstatus::Result fetchInto(const AppState& st, lyricbuf::Lyrics& out, int attempt) {
    using lyricstatus::Result;
    lyricbuf::reset(out);
    if (!st.trackName[0]) return Result::NotFound;
    String artist = urlEncode(st.artist);
    lrcmatch::Answer get{0, false, lrcstream::Kind::None}, found = get;
    const char* via = "get";
    get.kind = request("https://lrclib.net/api/get?track_name=" + urlEncode(st.trackName) +
                           "&artist_name=" + artist + "&album_name=" + urlEncode(st.album) +
                           "&duration=" + String(st.durationMs / 1000),
                       lrcstream::Mode::Get, st.durationMs, nullptr, false, out, &get.rc, &get.stalled);
    const bool searched = lrcmatch::needSearch(get);
    if (searched) {   // words match anywhere, so entries are chosen by duration and title
        char base[sizeof(st.trackName)];
        lrcmatch::baseTitle(st.trackName, base, sizeof(base));
        via = "search";
        found.kind = request("https://lrclib.net/api/search?track_name=" + urlEncode(base) +
                                 "&artist_name=" + artist,
                             lrcstream::Mode::Search, st.durationMs, st.trackName,
                             get.kind == lrcstream::Kind::Plain, out, &found.rc, &found.stalled);
    }
    Result r = lrcmatch::settle(get, searched ? &found : nullptr, attempt >= netplan::LyricsRetry::MAX_ATTEMPTS);
    if (r == Result::Plain) lyricbuf::spreadPlain(out, st.durationMs);
    const lrcmatch::Answer& last = searched ? found : get;

    char what[40];
    switch (r) {
        case Result::Synced:       snprintf(what, sizeof(what), "synced %d lines", out.n); break;
        case Result::Plain:        snprintf(what, sizeof(what), "plain %d lines", out.n); break;
        case Result::Instrumental: snprintf(what, sizeof(what), "instrumental"); break;
        case Result::NotFound:     snprintf(what, sizeof(what), "not found"); break;
        case Result::TempError:
            if (last.stalled) snprintf(what, sizeof(what), "temp error (stalled)");
            else snprintf(what, sizeof(what), "temp error rc=%d", last.rc);
            break;
    }
    Serial.printf("[lyrics] \"%s\" / %s: %s via %s (try %d)\n", st.trackName, st.artist, what, via, attempt);
    return r;
}

}
