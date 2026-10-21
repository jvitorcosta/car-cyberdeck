#include "playerjson.h"
#include <cstdio>
#include <cstring>
#include "jsonpath.h"
#include "jsonflat.h"

namespace playerjson {

// Number of elements in the array at `path` (0 when missing), up to a sane bound.
static int count(const char* body, const char* path) {
    char p[64];
    int n = 0;
    while (n < 16) {
        snprintf(p, sizeof p, "%s.%d", path, n);
        if (!jsonpath::find(body, p)) break;
        ++n;
    }
    return n;
}

// The image list's ~300 px url: Spotify lists them largest first (640, 300, 64), so the second
// smallest, as the deck has always shown; the only one when there is one.
static void pickArt(const char* body, const char* images, char* out, size_t n) {
    const int k = count(body, images);
    if (k == 0) return;
    char p[64];
    snprintf(p, sizeof p, "%s.%d.url", images, k >= 2 ? k - 2 : 0);
    jsonpath::str(body, p, out, n);
}

bool parse(const char* body, Now& out) {
    out = Now{};
    out.volume = -1;
    if (!jsonpath::boolean(body, "is_playing", &out.playing)) return false;
    long v = 0;
    if (jsonpath::num(body, "progress_ms", &v) && v > 0) out.progressMs = (uint32_t)v;
    jsonpath::boolean(body, "shuffle_state", &out.shuffle);
    char repeat[12];
    if (jsonpath::str(body, "repeat_state", repeat, sizeof repeat))
        out.repeat = strcmp(repeat, "track") == 0 ? 2 : strcmp(repeat, "context") == 0 ? 1 : 0;
    jsonpath::str(body, "device.name", out.deviceName, sizeof out.deviceName);
    jsonpath::str(body, "device.type", out.deviceType, sizeof out.deviceType);
    if (jsonpath::num(body, "device.volume_percent", &v)) out.volume = (int)v;
    jsonpath::str(body, "context.uri", out.context, sizeof out.context);

    char type[12] = "";
    jsonpath::str(body, "currently_playing_type", type, sizeof type);
    const char* item = jsonpath::find(body, "item");
    if (!item || *item != '{') {
        out.kind = strcmp(type, "ad") == 0 ? Kind::Other : Kind::None;
        return true;
    }
    if (strcmp(type, "track") == 0) {
        out.kind = Kind::Track;
        jsonpath::str(body, "item.artists.0.name", out.artist, sizeof out.artist);
        jsonpath::str(body, "item.album.name", out.album, sizeof out.album);
        pickArt(body, "item.album.images", out.artUrl, sizeof out.artUrl);
    } else if (strcmp(type, "episode") == 0) {
        out.kind = Kind::Episode;
        jsonpath::str(body, "item.show.name", out.artist, sizeof out.artist);
        pickArt(body, "item.images", out.artUrl, sizeof out.artUrl);
        if (!out.artUrl[0]) pickArt(body, "item.show.images", out.artUrl, sizeof out.artUrl);
    } else {
        out.kind = Kind::Other;
        return true;
    }
    jsonpath::str(body, "item.name", out.name, sizeof out.name);
    jsonpath::str(body, "item.uri", out.uri, sizeof out.uri);
    if (jsonpath::num(body, "item.duration_ms", &v) && v > 0) out.durationMs = (uint32_t)v;
    return true;
}

}
