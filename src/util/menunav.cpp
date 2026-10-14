#include "menunav.h"

namespace menunav {
int buttons(Screen s, bool vroom, Btn out[MAX_BTNS]) {
    int n = 0;
    switch (s) {
    case Screen::Home:                         // just her: the phone page opens the screens
        break;
    case Screen::Lyrics:                       // under her 1x portrait and name tag
        out[n++] = {Back, {4, 110, 92, 36}, "BACK"};
        break;
    case Screen::Sounds:
        out[n++] = {Howl, {20, 16, 280, 64}, "HOWL"};
        if (vroom) out[n++] = {Vroom, {20, 90, 280, 64}, "VROOM"};
        out[n++] = {Back, {20, 164, 100, 36}, "BACK"};
        break;
    case Screen::Trip:
        out[n++] = {Reset, {20, 130, 170, 60}, "RESET (hold)"};
        out[n++] = {Back, {200, 130, 100, 60}, "BACK"};
        break;
    }
    return n;
}

Button hit(Screen s, bool vroom, int x, int y) {
    Btn b[MAX_BTNS];
    const int n = buttons(s, vroom, b);
    for (int i = 0; i < n; ++i) {
        const Rect& r = b[i].r;
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) return b[i].id;
    }
    return None;
}

bool isHold(Button b) { return b == Reset; }

Screen next(Screen s, Button b) {
    switch (b) {
    case Lyrics: return s == Screen::Home ? Screen::Lyrics : s;
    case Sounds: return s == Screen::Home ? Screen::Sounds : s;
    case Trip: return s == Screen::Home ? Screen::Trip : s;
    case Back: return Screen::Home;
    default: return s;
    }
}

const char* idleText(Screen s, bool deckOk, bool wrongKey) {
    if (wrongKey) return "Wrong key: REMOTE_TOKEN must match";
    if (!deckOk) return "Looking for the deck...";
    switch (s) {
    case Screen::Home: return "What shall we do, darling?";
    case Screen::Sounds: return "Pick a sound, darling!";
    case Screen::Trip: return "Hold RESET to start a new trip.";
    default: return "";
    }
}
}
