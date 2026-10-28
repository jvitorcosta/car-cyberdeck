#include "apple.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "../../common/net/http_config.h"
#include "../../common/core/watchdog.h"
#include "../../util/black/genre.h"
#include "../../util/common/text.h"
#include "../../util/common/readguard.h"

namespace applegenre {

// Static: off the 10 KB network-task stack (one lookup at a time, on that task).
static char s_body[1024];   // answers are ~300 B
static char s_term[300];
static char s_url[400];
// The answer is ~300 B: a 3 s pause means the link is bad, and the whole lookup gets 6 s (it
// used to be able to hold the network task ~24 s: 8 s handshake + 8 s GET + 8 s stall).
static constexpr uint32_t GENRE_STALL_MS = 3000, GENRE_DEADLINE_MS = 6000;

Result lookup(const char* artist, uint32_t* genreId, int* rc) {
    *rc = 0;
    if (!artist || !artist[0]) return Result::None;
    if (!netcfg::linkUp()) { *rc = -1; return Result::Error; }
    txt::urlEncode(artist, s_term, sizeof(s_term));
    snprintf(s_url, sizeof(s_url),
             "https://itunes.apple.com/search?term=%s&entity=musicArtist&limit=1&country=BR", s_term);
    WiFiClientSecure client;
    netcfg::secure(client);
    HTTPClient https;
    netcfg::streamed(https);
    if (!https.begin(client, s_url)) { *rc = -1; return Result::Error; }
    https.addHeader("User-Agent", netcfg::USER_AGENT);
    watchdog::feed();   // the request is bounded by its own timeouts (common/core/watchdog)
    *rc = https.GET();
    if (*rc != 200) { https.end(); return Result::Error; }
    WiFiClient* s = https.getStreamPtr();
    size_t len = 0;
    readguard::Guard guard(millis(), GENRE_STALL_MS, GENRE_DEADLINE_MS);
    while (len < sizeof(s_body) - 1 && (https.connected() || s->available())) {
        int a = s->available();
        if (a <= 0) {
            if (guard.check(millis()) != readguard::State::Ok) break;   // parse what arrived
            delay(1);
            continue;
        }
        size_t want = sizeof(s_body) - 1 - len;
        if ((size_t)a < want) want = (size_t)a;
        len += s->readBytes(s_body + len, want);
        guard.onData(millis());
        if (guard.check(millis()) == readguard::State::Deadline) break;
    }
    https.end();
    s_body[len] = '\0';
    switch (genre::readAnswer(s_body, genreId)) {
        case genre::Answer::Found:   return Result::Found;
        case genre::Answer::NoMatch: return Result::None;   // complete answer: Apple has no genre
        default:                     *rc = -2; return Result::Error;   // cut off: not cached
    }
}

}
