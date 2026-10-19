#pragma once
#include <cstdint>
// The S3's FT6336U touch controller (I2C 0x38): its report registers -> a touch point on the
// 320x240 screen (TFT rotation 1). PURE, host-tested; board/s3/touch_ft6336 does the I2C.
namespace ft6336 {
constexpr uint8_t ADDR = 0x38;
constexpr uint8_t REG_REPORT = 0x02;   // TD_STATUS, then P1_XH, P1_XL, P1_YH, P1_YL
constexpr int REPORT_LEN = 5;
constexpr uint8_t REG_CHIP_ID = 0xA3;  // 0x64 on the FT6336U

struct Raw { bool down; int x, y; };   // the panel's own portrait axes, 240 x 320
// regs: REPORT_LEN bytes from REG_REPORT. Down for 1-2 touches (the first one), not for 0 or
// a bad count (0x0F after power-up).
Raw parse(const uint8_t* regs);

// How the panel's axes sit on the landscape screen (checked on the deck by tapping corners).
struct Orient { bool swap, flipX, flipY; };
// Raw -> screen x 0..319, y 0..239, clamped.
void toScreen(const Raw& r, const Orient& o, int* x, int* y);
}
