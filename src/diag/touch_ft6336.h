#pragma once
// The S3's FT6336U touchscreen, polled over I2C (0x38, the bus it shares with the ES8311 codec;
// Arduino's Wire locks each transfer). UI loop only. Parsing and the screen mapping are in
// util/black/ft6336 (host-tested). Only the mic diag firmwares
// (micrec_s3, micmeter_s3) use it; the deck firmware has no touch.
namespace touchft {
bool begin();                  // probes the chip and logs it; false: no touch this boot
bool read(int* x, int* y);     // true while a finger is down, at screen x 0..319, y 0..239
}
