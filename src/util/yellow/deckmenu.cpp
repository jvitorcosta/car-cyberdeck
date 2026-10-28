#include "deckmenu.h"

namespace deckmenu {

namespace {

constexpr int SNAP = 10;
constexpr Rect NONE{0, 0, 0, 0};

bool built(const Rect& r) { return r.w > 0 && r.h > 0; }
bool inside(const Rect& r, int x, int y) { return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h; }

bool displayOnly(Screen s, int x, int y) {
    if (s == Screen::Menu) {
        return inside(HEADER, x, y) || inside(CLOCK_BAND, x, y) || inside(TRIP_BAND, x, y) || inside(LIGHTS_BAND, x, y) ||
               inside(PROMPT_BAND, x, y);
    }
    if (s == Screen::Karaoke) return inside(KARAOKE_BODY, x, y);
    if (s == Screen::Presets) return inside(STATUS, x, y);
    return inside(STATUS, x, y) || inside(LEVEL, x, y);
}

Rect mk(int x, int y, int w, int h) {
    return {static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<int16_t>(w), static_cast<int16_t>(h)};
}

}  // namespace

int count(Screen s) {
    if (s == Screen::Menu) return M_N;
    if (s == Screen::Karaoke) return K_N;
    if (s == Screen::Presets) return P_N;
    return L_N;
}

Rect rect(Screen s, int id) {
    if (id < 0 || id >= count(s)) return NONE;
    if (s == Screen::Menu) return mk(4 + (id - M_ITEM0) * 106, 192, 100, 44);  // LIGHTS, KARAOKE, MUSIC
    if (s == Screen::Karaoke) return mk(4, 2, 70, 36);                        // K_BACK, where Lights has its Back
    // Lights and Presets: < BACK, then the four tabs in x 80..316 (56 px: five 70 px buttons don't fit)
    if (id == L_BACK) return mk(4, 2, 70, 36);
    if (id < L_TAB0 + N_TABS) return mk(80 + (id - L_TAB0) * 60, 2, 56, 36);
    if (s == Screen::Presets) {  // two rows of three, centred under STATUS
        const int i = id - P_PRE0;
        return mk(4 + (i % 3) * 106, 80 + (i / 3) * 70, 100, 60);
    }
    if (id < L_MINUS) {  // the 2x4 colours, down to the level row
        const int i = id - L_TILE0;
        return mk(4 + (i % 4) * 79, 54 + (i / 4) * 68, 75, 64);
    }
    if (id == L_MINUS) return mk(4, 190, 96, 46);
    return mk(220, 190, 96, 46);  // L_PLUS
}

// The distance to a button is the Chebyshev gap from the point to its rect (0 inside it).
int hit(Screen s, int x, int y) {
    if (displayOnly(s, x, y)) return -1;
    int best = -1, bestD = SNAP + 1;
    const int n = count(s);
    for (int id = 0; id < n; id++) {
        const Rect r = rect(s, id);
        if (!built(r)) continue;
        const int dx = x < r.x ? r.x - x : x >= r.x + r.w ? x - (r.x + r.w - 1) : 0;
        const int dy = y < r.y ? r.y - y : y >= r.y + r.h ? y - (r.y + r.h - 1) : 0;
        const int d = dx > dy ? dx : dy;
        if (d < bestD) {
            bestD = d;
            best = id;
        }
    }
    return best;
}

int neighbour(Screen s, int from, int dx, int dy) {
    const Rect a = rect(s, from);
    const int ax = a.x + a.w / 2, ay = a.y + a.h / 2;
    int best = from, bestScore = 1 << 30;
    const int n = count(s);
    for (int id = 0; id < n; id++) {
        if (id == from) continue;
        const Rect b = rect(s, id);
        if (!built(b)) continue;
        const int vx = b.x + b.w / 2 - ax, vy = b.y + b.h / 2 - ay;
        const int along = dx ? vx * dx : vy * dy;
        if (along <= 0) continue;
        const int across = dx ? (vy < 0 ? -vy : vy) : (vx < 0 ? -vx : vx);
        if (dx && across > along) continue;  // left / right: more up or down than that way is not a neighbour
        if (along + 2 * across < bestScore) {
            bestScore = along + 2 * across;
            best = id;
        }
    }
    return best;
}

int presetSlot(Screen s, int id) {
    if (s != Screen::Presets) return -1;
    return id >= P_PRE0 && id < P_N ? id - P_PRE0 : -1;
}

bool isPreset(Screen s, int id) { return presetSlot(s, id) >= 0; }

}  // namespace deckmenu
