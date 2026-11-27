#pragma once
#include <Arduino_GFX_Library.h>
#include "panel.h"
// The panel's driver, for code that draws on it directly (the opening, the status screen).
namespace panel {
Arduino_GFX& gfx();
}
