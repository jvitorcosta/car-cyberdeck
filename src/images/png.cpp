#include "png.h"
#include <new>
#include "../core/mem.h"
#include "fetch.h"

namespace img {

static PNG* s_png = nullptr;

bool acquire() {
    if (s_png) return true;
    void* m = mem::bigAlloc(sizeof(PNG));   // ~45 KB, in PSRAM: no longer competes with TLS
    if (!m) return false;
    s_png = new (m) PNG();
    return true;
}

void release() {
    if (!s_png) return;
    s_png->~PNG();
    free(s_png);
    s_png = nullptr;
}

PNG& decoder() { return *s_png; }

bool loadSpriteInto(const char* url, uint8_t* buf, size_t cap, size_t* outLen) {
    return fetch::httpsGetInto(url, buf, cap, outLen);
}

}
