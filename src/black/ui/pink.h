#pragma once
// The black deck's pink, one value for its screens. The yellow deck's pink tile is ledlook's 0xFF0090;
// this panel (IPS, BGR) shows the same number less pink, so black gets more blue (picked by eye).
#include <stdint.h>
namespace ui {
constexpr uint32_t PINK_RGB = 0xFF00B4;
}
