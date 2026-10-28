#pragma once
#include <cstdint>
#include "../../util/black/qrfit.h"
// The phone-remote link's QR code on "Waiting for a song..." (S3): a Gen-3 battle box at the
// top left (ui::battleBox, slanted right edge and drop shadow), its name line a phone icon and
// "REMOTE" in font 2, the code below on a cream panel at 3 px a module with a 2-module quiet
// zone (util/qrfit). ui::drawQrCard paints it when the screen appears and again only when the
// code changes; the frames never reach it (statusscene::frameRect). PURE, host-tested.
namespace qrcard {
struct Qr { const uint8_t* bits; int size; };   // modules row-major, MSB first, 1 = dark
struct Rect { int x, y, w, h; };

constexpr int PANEL = qrfit::pixels(qrfit::MAX_VERSION);   // 99: a version 3 code and its quiet zone
constexpr int BORDER = 2, PAD = 4, NAME_H = 18;           // battle-box border, panel margin, name line
constexpr int TAB = 10;                                    // ui::battleBox's slant
constexpr int SHADOW = 3;                                  // and its drop shadow
constexpr Rect BOX = {4, 22, BORDER + PAD + PANEL + PAD + BORDER + TAB,
                      BORDER + NAME_H + PANEL + PAD + BORDER};
constexpr Rect PANEL_AT = {BOX.x + BORDER + PAD, BOX.y + BORDER + NAME_H, PANEL, PANEL};
constexpr Rect FOOTPRINT = {BOX.x, BOX.y, BOX.w + SHADOW, BOX.h + SHADOW};   // x 4..127, y 22..149
constexpr int ICON_X = BOX.x + 7, ICON_Y = BOX.y + 5;      // the phone icon (12 px)
constexpr int LABEL_X = BOX.x + 22, LABEL_Y = BOX.y + 3;   // "REMOTE", font 2

// Where the code's first module sits inside the panel, in px from its left and top edges:
// centred, so a smaller version gets a wider quiet zone. -1 for a size the panel can't hold.
int offset(int size);
// Module (mx, my) is dark; false outside the code.
bool dark(const Qr& qr, int mx, int my);
// A frame's repaint box with the card on screen: the columns right of the card when it reaches
// into it (Pikachu's tail), else unchanged. The card stays on top.
Rect beside(const Rect& r);
}
