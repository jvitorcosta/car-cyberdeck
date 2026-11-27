#include "canvas.h"
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
}  // namespace canvas
