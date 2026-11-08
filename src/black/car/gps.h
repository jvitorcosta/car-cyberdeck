#pragma once
#include "../../util/car/cameras.h"
// The phone's GPS on the black deck: the owner's iPhone runs GPS 2 IP, which serves NMEA ($GPRMC,
// about 1 Hz) on a plain TCP socket at GPS_HOST:GPS_PORT (the hotspot's own address by default; on
// the bench, a PC running tools/nmea_replay.py). One small task (4 KB stack, priority 1, core 0)
// keeps the socket and feeds every byte to nmea::Reader. One plain socket, no TLS: it never takes
// from the heap Spotify's handshakes need.
namespace gps {
// Called from the GPS task for every RMC sentence (valid or not), and with valid = false once a
// second while none arrives (WiFi down, the app closed, the socket quiet), so the matcher can
// still tell "no fix". Keep it short: it runs on the GPS task's 4 KB stack.
using Sink = void (*)(const cameras::Fix& f);
// Starts the task. Call once; it connects only while WiFi is up.
void start(Sink sink);
}  // namespace gps
