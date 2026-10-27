#include "cjkdata.h"
#include <Arduino.h>
#include "../core/media.h"

MEDIA_DECL(data_cjk16_bin);

namespace ui {

const cjkfont::Font& cjk() {
    static cjkfont::Font font;
    static bool opened = false;
    if (!opened) {
        opened = true;
        const media::Blob b = MEDIA(data_cjk16_bin, "cjk16.bin");
        if (!font.open(b.data, b.size))
            Serial.printf("[cjk] font blob invalid (%u bytes)\n", (unsigned)b.size);
    }
    return font;
}

}
