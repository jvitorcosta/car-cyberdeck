#pragma once
#include "emojifont.h"
namespace ui {
// The Twemoji font in flash (data/emoji16.bin). Empty font if the blob is invalid or missing.
const emojifont::Font& emoji();
}
