#include "status_sprite.h"

namespace status_sprite {

struct Sprite { int w, h; const uint16_t* px; const uint8_t* mask; const char* name; int dex; };
#include "status_sprite.inc"
static_assert(sizeof(SPRITES) / sizeof(SPRITES[0]) == COUNT, "regenerate status_sprite.inc");

static const Sprite* get(int id) { return id >= 0 && id < COUNT ? &SPRITES[id] : nullptr; }

int width(int id) { const Sprite* s = get(id); return s ? s->w : 0; }
int height(int id) { const Sprite* s = get(id); return s ? s->h : 0; }
const char* name(int id) { const Sprite* s = get(id); return s ? s->name : ""; }
int dex(int id) { const Sprite* s = get(id); return s ? s->dex : 0; }

bool pixel(int id, int x, int y, uint16_t* rgb565) {
    const Sprite* s = get(id);
    if (!s || x < 0 || y < 0 || x >= s->w || y >= s->h) return false;
    int i = y * s->w + x;
    if (!((s->mask[i >> 3] >> (7 - (i & 7))) & 1)) return false;
    if (rgb565) *rgb565 = s->px[i];
    return true;
}

}
