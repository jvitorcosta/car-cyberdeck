#include "statusscene.h"
#include "status_sprite.h"
#include "pokeball.h"

namespace statusscene {

Look lookFor(linkstatus::Kind k) {
    switch (k) {
    case linkstatus::Kind::NoWifi:       return {status_sprite::ZUBAT, Bubble::Question, true};   // searching
    case linkstatus::Kind::NoInternet:   return {status_sprite::PSYDUCK, Bubble::Dots, false};    // puzzled
    case linkstatus::Kind::SpotifyError: return {status_sprite::SLOWPOKE, Bubble::Zzz, false};    // waiting
    case linkstatus::Kind::CertRejected: return {status_sprite::PORYGON, Bubble::Bang, false};    // alarm
    case linkstatus::Kind::Ok:           break;
    }
    return {status_sprite::PIKACHU, Bubble::None, false};
}

static constexpr int CX = 160;                    // sprite and platform centre
static constexpr int PLAT_Y = 136, PLAT_RX = 60, PLAT_RY = 12;
static constexpr int BALL_TOP = FEET_Y - 1;
static constexpr int BALL_RANGE = 48, BALL_STEP = 6, BALL_STEPS = 2 * BALL_RANGE / BALL_STEP;
static constexpr int GLYPH_ADV = 12;              // 5x7 font at 2x, 1 px gap (2x)
static constexpr int BUBBLE_W = 46, BUBBLE_H = 22;

Pose poseAt(uint32_t ms, const Look& look) {
    static const int BOB[] = {0, 1, 2, 1};
    Pose p{};
    p.step = (int)(ms / FRAME_MS);
    p.bob = BOB[p.step & 3];
    if (look.ball) {
        int pos = p.step % (2 * BALL_STEPS);
        p.ballX = pos < BALL_STEPS ? -BALL_RANGE + pos * BALL_STEP
                                   : BALL_RANGE - (pos - BALL_STEPS) * BALL_STEP;
        p.ballFrame = ((p.ballX + BALL_RANGE) / BALL_STEP) % pokeball::FRAMES;   // rolls, not slides
    }
    return p;
}

// 5x7 glyphs, bit 4 = left column.
static const uint8_t* glyph(char ch) {
    static const uint8_t Q[7]    = {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04};
    static const uint8_t DOT[7]  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
    static const uint8_t z[7]    = {0x00, 0x00, 0x1F, 0x02, 0x04, 0x08, 0x1F};
    static const uint8_t Z[7]    = {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};
    static const uint8_t BANG[7] = {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04};
    switch (ch) {
    case '?': return Q;
    case '.': return DOT;
    case 'z': return z;
    case 'Z': return Z;
    case '!': return BANG;
    }
    return nullptr;
}

// The bubble's text for this frame: dots and z's build up one at a time.
static const char* bubbleText(Bubble b, int step) {
    static const char* const DOTS[] = {".", "..", "...", "..."};
    static const char* const ZZZ[] = {"z", "zZ", "zZz", "zZz"};
    switch (b) {
    case Bubble::Question: return "?";
    case Bubble::Dots:     return DOTS[step & 3];
    case Bubble::Zzz:      return ZZZ[step & 3];
    case Bubble::Bang:     return "!";
    case Bubble::None:     break;
    }
    return "";
}

static int spriteTop(const Look& look, const Pose& pose) {
    return FEET_Y - 2 * status_sprite::height(look.sprite) - pose.bob;
}

Rect bubbleRect(const Look& look, const Pose& pose) {
    Rect r{CX + status_sprite::width(look.sprite) - 6, spriteTop(look, pose) - BUBBLE_H + 6,
           BUBBLE_W, BUBBLE_H};
    if (r.y < Y0) r.y = Y0;
    return r;
}

// Bubble box (rounded white, ink border, text) and its tail pointing down-left at the sprite.
static bool bubblePixel(int x, int y, const Look& look, const Pose& pose, const Colors& c,
                        uint16_t* out) {
    if (look.bubble == Bubble::None) return false;
    const Rect r = bubbleRect(look, pose);
    const int ty = y - (r.y + r.h);   // tail rows under the box
    if (ty >= 0 && ty < 4) {
        int tx = x - (r.x + 4);
        if (tx >= ty && tx <= 5 - ty) { *out = (tx == ty || tx == 5 - ty || ty == 3) ? c.ink : c.bubble; return true; }
        return false;
    }
    const int bx = x - r.x, by = y - r.y;
    if (bx < 0 || by < 0 || bx >= r.w || by >= r.h) return false;
    const bool edgeX = bx == 0 || bx == r.w - 1, edgeY = by == 0 || by == r.h - 1;
    if (edgeX && edgeY) return false;                                // rounded corner
    if (edgeX || edgeY) { *out = c.ink; return true; }
    *out = c.bubble;
    const char* s = bubbleText(look.bubble, pose.step);
    int n = 0;
    while (s[n]) ++n;
    const int textW = n * GLYPH_ADV - 2;
    const int gx0 = (r.w - textW) / 2, gy0 = (r.h - 14) / 2;
    const int gx = bx - gx0, gy = by - gy0;
    if (gx >= 0 && gy >= 0 && gy < 14 && gx < textW) {
        const int cell = gx / GLYPH_ADV, cx = (gx % GLYPH_ADV) / 2;
        const uint8_t* g = glyph(s[cell]);
        if (g && cx < 5 && ((g[gy / 2] >> (4 - cx)) & 1)) *out = c.ink;
    }
    return true;
}

static bool ballPixel(int x, int y, const Look& look, const Pose& pose, const Colors& c,
                      uint16_t* out) {
    if (!look.ball) return false;
    const int bx = x - (CX + pose.ballX - pokeball::SIZE / 2), by = y - BALL_TOP;
    if (bx < 0 || by < 0 || bx >= pokeball::SIZE || by >= pokeball::SIZE) return false;
    switch (pokeball::pixel(bx, by, pose.ballFrame)) {
    case pokeball::Px::Red:   *out = c.ballRed; return true;
    case pokeball::Px::White: *out = c.ballWhite; return true;
    case pokeball::Px::Dark:  *out = c.ballDark; return true;
    case pokeball::Px::Clear: break;
    }
    return false;
}

static bool insideEllipse(int dx, int dy, int rx, int ry) {
    return (long)dx * dx * ry * ry + (long)dy * dy * rx * rx <= (long)rx * rx * ry * ry;
}

uint16_t pixel(int x, int y, const Look& look, const Pose& pose, const Colors& c) {
    uint16_t v;
    if (bubblePixel(x, y, look, pose, c, &v)) return v;
    if (ballPixel(x, y, look, pose, c, &v)) return v;
    const int w = status_sprite::width(look.sprite);
    const int left = CX - w, top = spriteTop(look, pose);
    if (x >= left && y >= top && status_sprite::pixel(look.sprite, (x - left) / 2, (y - top) / 2, &v))
        return v;
    const int dx = x - CX, dy = y - PLAT_Y;
    if (insideEllipse(dx, dy, PLAT_RX, PLAT_RY))
        return insideEllipse(dx, dy, PLAT_RX - 1, PLAT_RY - 1) ? c.platform : c.edge;
    if (y < 118) return c.sky;      // same bands as ui::background()
    if (y < 122) return c.horizon;
    return c.grass;
}

}
