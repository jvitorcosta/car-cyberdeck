#pragma once
#include <cstdint>
// The Pokemon shown on the status screens, bundled in flash (tools/gen_status_sprite.py):
// those screens appear exactly when nothing can be downloaded. ui/statusscene picks which
// one shows when. PURE, host-tested.
namespace status_sprite {
enum Id : int { PIKACHU, ZUBAT, PSYDUCK, SLOWPOKE, PORYGON, COUNT };   // generator's order
int width(int id);    // 0 for an unknown id
int height(int id);
// True when (x, y) of sprite `id` is opaque; then *rgb565 (if given) is its RGB565 colour.
bool pixel(int id, int x, int y, uint16_t* rgb565);
const char* name(int id);   // "SLOWPOKE" (info box); "" for an unknown id
int dex(int id);            // National Dex number (the box's "Lv"); 0 for an unknown id
}
