#include "cjkdata.h"
#include <Arduino.h>
#include "../core/embed.h"

EMBED(data_cjk16_bin);

namespace ui {

const cjkfont::Font& cjk() {
    static cjkfont::Font font;
    static bool opened = false;
    if (!opened) {
        opened = true;
        size_t len = EMBED_SIZE(data_cjk16_bin);
        if (!font.open(EMBED_DATA(data_cjk16_bin), len))
            Serial.printf("[cjk] font blob invalid (%u bytes)\n", (unsigned)len);
    }
    return font;
}

}
