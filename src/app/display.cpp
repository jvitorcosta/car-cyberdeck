#include "display.h"

static TFT_eSPI s_tft = TFT_eSPI();

namespace app {
TFT_eSPI& tft() { return s_tft; }
}
