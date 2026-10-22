#include "emojidata.h"
#include <Arduino.h>
#include "../core/media.h"

MEDIA_DECL(data_emoji16_bin);

namespace ui {

const emojifont::Font& emoji() {
    static emojifont::Font font;
    static bool opened = false;
    if (!opened) {
        opened = true;
        const media::Blob b = MEDIA(data_emoji16_bin, "emoji16.bin");
        if (!font.open(b.data, b.size))
            Serial.printf("[emoji] font blob invalid (%u bytes)\n", (unsigned)b.size);
    }
    return font;
}

}
