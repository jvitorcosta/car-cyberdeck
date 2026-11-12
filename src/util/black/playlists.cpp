#include "playlists.h"
#include <cstdio>
#include <cstring>
#include "../common/jsonpath.h"

namespace playlists {

static bool base62(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
}

bool validId(const char* id) {
    if (!id) return false;
    size_t i = 0;
    for (; id[i]; ++i)
        if (i >= ID_LEN || !base62(id[i])) return false;
    return i == ID_LEN;
}

bool parseOffset(const char* s, int* out) {
    if (!s || !*s || strlen(s) > 4) return false;
    int v = 0;
    for (const char* p = s; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
        v = v * 10 + (*p - '0');
    }
    if (v > MAX_OFFSET) return false;
    *out = v;
    return true;
}

int writePage(const char* body, int offset, char* out, size_t n) {
    if (n) out[0] = '\0';
    long total = 0;
    if (!body || !jsonpath::find(body, "items") || !jsonpath::num(body, "total", &total)) return -1;
    int items = 0;
    char path[24];
    for (; items < 50; ++items) {   // Spotify's own limit is 50 a page
        snprintf(path, sizeof path, "items.%d", items);
        if (!jsonpath::find(body, path)) break;
    }
    int len = snprintf(out, n, "playlists %ld %d\n", total, offset + items);
    if (len < 0 || (size_t)len >= n) { if (n) out[0] = '\0'; return -1; }
    int written = 0;
    for (int i = 0; i < items; ++i) {
        char id[ID_LEN + 2], name[160];   // Spotify's names stop at 100 characters
        snprintf(path, sizeof path, "items.%d.id", i);
        if (!jsonpath::str(body, path, id, sizeof id) || !validId(id)) continue;   // a null item (deleted)
        snprintf(path, sizeof path, "items.%d.name", i);
        if (!jsonpath::str(body, path, name, sizeof name) || !name[0]) strcpy(name, "-");
        // cut here, not by the read: a character that doesn't fit there is skipped, not the end
        if (strlen(name) > NAME_BYTES) {
            size_t cut = NAME_BYTES;
            while (cut > 0 && ((unsigned char)name[cut] & 0xC0) == 0x80) --cut;   // back to a character's start
            name[cut] = '\0';
        }
        for (char* p = name; *p; ++p)
            if ((unsigned char)*p < 0x20 || *p == 0x7F) *p = ' ';   // one line each
        const int k = snprintf(out + len, n - len, "%s %s\n", id, name);
        if (k < 0 || (size_t)k >= n - len) { out[0] = '\0'; return -1; }
        len += k;
        ++written;
    }
    return written;
}

bool playBody(const char* id, char* out, size_t n) {
    if (n) out[0] = '\0';
    if (!validId(id)) return false;
    const int len = snprintf(out, n, "{\"context_uri\":\"spotify:playlist:%s\"}", id);
    if (len < 0 || (size_t)len >= n) { if (n) out[0] = '\0'; return false; }
    return true;
}

bool playBodyAt(const char* id, int position, char* out, size_t n) {
    if (position < 0) return playBody(id, out, n);
    if (n) out[0] = '\0';
    if (!validId(id)) return false;
    const int len = snprintf(out, n, "{\"context_uri\":\"spotify:playlist:%s\",\"offset\":{\"position\":%d}}", id, position);
    if (len < 0 || (size_t)len >= n) { if (n) out[0] = '\0'; return false; }
    return true;
}

bool readTotal(const char* body, int* total) {
    long v = 0;
    if (!body || !(jsonpath::num(body, "items.total", &v) || jsonpath::num(body, "tracks.total", &v)) || v < 0)
        return false;
    *total = (int)v;
    return true;
}

bool pickDevice(const char* body, char* out, size_t n) {
    if (n) out[0] = '\0';
    if (!body) return false;
    // rank 2: active, 1: a phone; never an idle PC or speaker (the music would play at home)
    int pick = -1, rank = 0;
    char path[40], type[24];
    for (int i = 0; i < 20; ++i) {
        snprintf(path, sizeof path, "devices.%d", i);
        if (!jsonpath::find(body, path)) break;
        bool active = false, restricted = false;
        snprintf(path, sizeof path, "devices.%d.is_restricted", i);
        if (jsonpath::boolean(body, path, &restricted) && restricted) continue;
        snprintf(path, sizeof path, "devices.%d.is_active", i);
        jsonpath::boolean(body, path, &active);
        snprintf(path, sizeof path, "devices.%d.type", i);
        jsonpath::str(body, path, type, sizeof type);
        const int r = active ? 2 : strcmp(type, "Smartphone") == 0 ? 1 : 0;
        if (r > rank) { rank = r; pick = i; }
    }
    if (pick < 0) return false;
    snprintf(path, sizeof path, "devices.%d.id", pick);
    return jsonpath::str(body, path, out, n) && out[0];
}

int startPosition(int total, unsigned random) { return total > 0 ? (int)(random % (unsigned)total) : -1; }

const char* why(int code, const char* body, bool play) {
    const bool has = body && *body;
    if (code < 0) return "Spotify not answering";
    if (has && strstr(body, "NO_ACTIVE_DEVICE")) return "Open Spotify on phone";
    if (has && strstr(body, "PREMIUM_REQUIRED")) return "Needs Spotify Premium";
    if (has && strstr(body, "scope")) return "Re-authorise Spotify";   // "Insufficient client scope"
    if (code == 404) return play ? "Open Spotify on phone" : "Not found";   // play's 404: no device to play on
    if (code == 401 || code == 400) return "Spotify login failed";   // 400: accounts' invalid_grant
    if (code == 403) return "Spotify refused";
    if (code == 429) return "Spotify busy: wait";
    if (code >= 500) return "Spotify down: retry";
    return "Spotify error";
}
}
