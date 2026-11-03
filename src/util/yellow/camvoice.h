#pragma once
// Spoken camera commands ("mark camera", "cameras mute", "radar on") -> what to ask the black deck.
// The text is whatever Siri dictated, English or Portuguese, any case, with punctuation, the same as
// ledvoice gets; /say tries this first and hands everything else to ledvoice. A phrase needs a camera
// word AND an action word, in either order: "camera" alone, or "mute" alone, is nothing, so no lights
// phrase can turn into a camera command ("lights off" has no camera word). The first action word
// wins. Words count whole only ("cameraman" is not "camera"). PURE, host-tested.
#include <cctype>
#include <cstring>

namespace camvoice {
enum class Cmd { None, Mark, Mute, Unmute };

namespace detail {
// "câmera" keeps its accent: the phrase isn't folded (same as ledvoice's "lilás").
static const char* const CAMERA[] = {
    "camera", "cameras", "c\xc3\xa2mera", "c\xc3\xa2meras", "radar", "radares", "radars",
};
struct Word { const char* text; Cmd cmd; };
// "off" mutes: the bands stay, only the black deck's beeps stop, which is what "cameras off" asks
// for while driving. "on" / "liga" undo it.
static const Word ACTIONS[] = {
    {"mark", Cmd::Mark},        {"marca", Cmd::Mark},       {"marcar", Cmd::Mark},     {"marque", Cmd::Mark},
    {"mute", Cmd::Mute},        {"silence", Cmd::Mute},     {"silenciar", Cmd::Mute},  {"silencia", Cmd::Mute},
    {"silencie", Cmd::Mute},    {"off", Cmd::Mute},         {"desliga", Cmd::Mute},
    {"unmute", Cmd::Unmute},    {"on", Cmd::Unmute},        {"liga", Cmd::Unmute},
};
inline bool isCamera(const char* w) {
    for (const char* c : CAMERA) {
        if (std::strcmp(w, c) == 0) return true;
    }
    return false;
}
inline Cmd action(const char* w) {
    for (const Word& a : ACTIONS) {
        if (std::strcmp(w, a.text) == 0) return a.cmd;
    }
    return Cmd::None;
}
}  // namespace detail

inline Cmd parse(const char* text) {
    if (!text) return Cmd::None;
    bool camera = false;
    Cmd cmd = Cmd::None;
    char w[16];
    for (const char* p = text; *p;) {
        // a word: letters, digits and UTF-8 bytes (accents); everything else separates
        int n = 0;
        bool tooLong = false;
        while (*p && (std::isalnum(static_cast<unsigned char>(*p)) || static_cast<unsigned char>(*p) >= 0x80)) {
            if (n < static_cast<int>(sizeof(w)) - 1) w[n++] = static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
            else tooLong = true;  // longer than any word we know: don't let its prefix match
            p++;
        }
        w[n] = 0;
        if (n == 0) {
            p++;
            continue;
        }
        if (tooLong) continue;
        if (detail::isCamera(w)) camera = true;
        else if (cmd == Cmd::None) cmd = detail::action(w);
    }
    return camera ? cmd : Cmd::None;
}
}  // namespace camvoice
