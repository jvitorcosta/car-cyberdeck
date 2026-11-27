#include "canvas.h"
#include <cstring>
#include "panel.h"

namespace canvas {
namespace {
TFT_eSPI s_driver;               // never init(): the sprite only needs its settings
TFT_eSprite s_frame(&s_driver);
uint16_t* s_pixels = nullptr;    // big-endian RGB565, as TFT_eSPI keeps a sprite
}  // namespace

bool begin() {
    s_frame.setColorDepth(16);
    s_pixels = static_cast<uint16_t*>(s_frame.createSprite(panel::W, panel::H));
    return s_pixels != nullptr;
}

TFT_eSPI& screen() { return s_frame; }

void present() {
    if (s_pixels) panel::push(s_pixels);
}
void blit(int x, int y, int w, int h, const uint16_t* bigEndian) {
    if (!s_pixels || !bigEndian) return;
    const int x0 = x < 0 ? -x : 0, x1 = x + w > panel::W ? panel::W - x : w;
    for (int row = 0; row < h && x1 > x0; ++row) {
        if (y + row < 0 || y + row >= panel::H) continue;
        memcpy(s_pixels + (y + row) * panel::W + x + x0, bigEndian + row * w + x0, (x1 - x0) * sizeof(uint16_t));
    }
}
}  // namespace canvas
