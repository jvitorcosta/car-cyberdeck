#pragma once
#include <TFT_eSPI.h>
// Desk commands on the black deck's USB serial, one line each, read from the UI loop:
//   GAUGES ON / GAUGES OFF   the GAUGES switch (car/gauges), as GET /gauges does
//   GAUGES SCREEN suki|drive|trip  the GAUGES screen (refused while the car moves), as GET /gauges?screen= does
//   GAUGES DEMO              the HUD with the OBD deck's simulated drive (util/car/benchdrive), RAM
//                            only, until GAUGES ON / OFF or a reboot: the look without the car
//   FILL / FILL 6.49         a full tank in the fuel book, with the price per litre, as GET /fill does
//   FILL CLEAR               the fuel book back to empty, after a desk test
//   TRIP                     the trip and fuel book values as live() has them, one "[obd] trip" line
//   SHOT                     the panel to the PC as rows (tools/deck_shot.py --black), the yellow
//                            deck's protocol: "SHOT 320 240", "R<y> <hex>" per row, "SHOT END", then
//                            "R<y>" asks a lost row again until "Q" or 2 s of silence
namespace serialcmd {
void tick(TFT_eSPI& tft);
}
