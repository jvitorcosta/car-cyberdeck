#include "camwire.h"
#include <cstdio>
#include <cstring>
#include "../common/jsonflat.h"

namespace camwire {

static const char* stageName(cameras::Stage s) {
    switch (s) {
        case cameras::Stage::Off: return "off";
        case cameras::Stage::NoFix: return "nofix";
        case cameras::Stage::Alert: return "alert";
        case cameras::Stage::Alarm: return "alarm";
    }
    return "off";
}

// Appends to out[*len], keeping room for the terminator; sets *ok false on overflow.
static void put(char* out, size_t n, size_t* len, const char* s, bool* ok) {
    size_t l = strlen(s);
    if (*len + l + 1 > n) { *ok = false; return; }
    memcpy(out + *len, s, l + 1);
    *len += l;
}

// A JSON string value: quotes and backslashes escaped, control chars as \u00XX, bytes >= 0x80 as-is.
static void putStr(char* out, size_t n, size_t* len, const char* s, bool* ok) {
    put(out, n, len, "\"", ok);
    for (; *s && *ok; s++) {
        unsigned char c = (unsigned char)*s;
        char buf[8];
        if (c == '"' || c == '\\') { buf[0] = '\\'; buf[1] = (char)c; buf[2] = 0; }
        else if (c < 0x20) snprintf(buf, sizeof buf, "\\u%04x", c);
        else { buf[0] = (char)c; buf[1] = 0; }
        put(out, n, len, buf, ok);
    }
    put(out, n, len, "\"", ok);
}

bool write(const cameras::Status& s, const camlist::Point* active, bool mute, char* out, size_t n) {
    if (!out || n == 0) return false;
    out[0] = 0;
    bool hasPoint = s.stage == cameras::Stage::Alert || s.stage == cameras::Stage::Alarm;
    if (hasPoint && !active) return false;
    size_t len = 0;
    bool ok = true;
    char buf[48];
    snprintf(buf, sizeof buf, "{\"st\":\"%s\"", stageName(s.stage));
    put(out, n, &len, buf, &ok);
    if (hasPoint) {
        snprintf(buf, sizeof buf, ",\"kind\":\"%s\",\"limit\":%u,\"dist\":%u", camlist::kindName(active->kind),
                 (unsigned)active->limit, (unsigned)s.distM);
        put(out, n, &len, buf, &ok);
    }
    snprintf(buf, sizeof buf, ",\"kmh\":%u", (unsigned)s.kmh);
    put(out, n, &len, buf, &ok);
    if (hasPoint) {
        put(out, n, &len, ",\"label\":", &ok);
        putStr(out, n, &len, active->label, &ok);
        put(out, n, &len, ",\"id\":", &ok);
        putStr(out, n, &len, active->id, &ok);
    }
    snprintf(buf, sizeof buf, ",\"mute\":%d}", mute ? 1 : 0);
    put(out, n, &len, buf, &ok);
    if (!ok) out[0] = 0;
    return ok;
}

bool parse(const char* json, View* out) {
    if (!json || !out) return false;
    char w[16];
    View v{};
    v.kind = camlist::Kind::Speed;
    if (!jsonflat::str(json, "st", w, sizeof w)) return false;
    if (!strcmp(w, "off")) v.stage = cameras::Stage::Off;
    else if (!strcmp(w, "nofix")) v.stage = cameras::Stage::NoFix;
    else if (!strcmp(w, "alert")) v.stage = cameras::Stage::Alert;
    else if (!strcmp(w, "alarm")) v.stage = cameras::Stage::Alarm;
    else return false;
    long x = 0;
    if (jsonflat::num(json, "kmh", &x) && x > 0) v.kmh = (uint16_t)(x > 65535 ? 65535 : x);
    bool m = false;
    if (jsonflat::boolean(json, "mute", &m)) v.mute = m;
    else if (jsonflat::num(json, "mute", &x)) v.mute = x != 0;
    if (v.stage == cameras::Stage::Alert || v.stage == cameras::Stage::Alarm) {
        if (!jsonflat::str(json, "kind", w, sizeof w)) return false;
        if (!strcmp(w, "speed")) v.kind = camlist::Kind::Speed;
        else if (!strcmp(w, "bump")) v.kind = camlist::Kind::Bump;
        else if (!strcmp(w, "redlight")) v.kind = camlist::Kind::RedLight;
        else return false;
        if (!jsonflat::num(json, "dist", &x) || x < 0) return false;
        v.distM = (uint16_t)(x > 65535 ? 65535 : x);
        if (jsonflat::num(json, "limit", &x) && x > 0) v.limit = (uint8_t)(x > 255 ? 255 : x);
        jsonflat::str(json, "label", v.label, sizeof v.label);
    }
    *out = v;
    return true;
}

}  // namespace camwire
