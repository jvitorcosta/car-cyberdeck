#pragma once
#include <cstdint>
// Display backlight on GPIO 21 (PIN_BL) through LEDC PWM: full by day, dimmer at night.
namespace backlight {
void begin();            // after tft.init() (which drives the pin high); starts at day level
void set(bool night);
// The LEDC hardware fades the backlight to off over `ms` while the CPU keeps drawing (the boot
// scene's brake: smooth, unlike dimming every pixel each frame). false: no fade (draw it yourself).
bool fadeOut(uint32_t ms);
void restore();          // back to the day/night level at once (after fadeOut)
}
