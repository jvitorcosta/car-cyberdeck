#pragma once
// The CYD's resistive touchscreen (XPT2046) for the companion's buttons. It sits on its own pins
// (include/pins.h); the companion drives it on VSPI because the display owns HSPI.
namespace touch {
void begin();
// True while pressed hard enough; *x, *y in screen coordinates. Logs the raw reading at each
// touch-down ("[touch] raw x=.. y=.. -> x,y") for calibration.
bool read(int* x, int* y);
}
