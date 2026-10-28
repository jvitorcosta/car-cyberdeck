#pragma once
#include <cstdint>
#include <esp_task_wdt.h>
// The task watchdog for the tasks that can hang, on both decks. The core only watches core 0's
// idle task, so a task blocked for good (a lock never released, a socket or NimBLE wait that never
// returns) or a spin on core 1 froze the deck for the rest of the drive. Each long-running task
// calls watch() as it starts and feed() once per pass of its loop; the loop tasks use the core's
// enableLoopWDT() (it feeds before each loop()). A task that is not watched can call feed():
// it does nothing there, so shared code (net/wifi's joins, the S3's requests) feeds whoever runs it.
//
// TIMEOUT_S is above the longest legitimate wait between two feeds, from the code's own timeouts:
//  - the S3 network task, one request (fed before each): DNS up to 15 s (lwIP's own limit) +
//    connect 5 s (HTTPClient) + TLS handshake 8 s (netcfg) + the head 8 s + an image body's 20 s
//    deadline = 56 s;
//  - the yellow deck's link task, one light (fed before each): connect ~13 s (4 s x 3 tries) + one
//    GATT discovery that runs into the 30 s ATT timeout = ~45 s; the Xbox controller (fed after):
//    two connects of ~16 s (5 s x 3) + one discovery into the ATT timeout = ~62 s;
//  - a WiFi join (fed before each try): 8 s + ~1.2 s of pings.
// 90 s leaves over 1.4x on the worst of them (1.6x on the S3's), so a slow hotspot or a slow light
// never trips it; a real hang reboots the deck within 90 s instead of never.
namespace watchdog {
constexpr uint32_t TIMEOUT_S = 90;
// Early in setup(): the timeout above, and a reset (panic) when it runs out.
inline void begin() { esp_task_wdt_init(TIMEOUT_S, true); }
// The calling task is watched from now on: it must feed() at least every TIMEOUT_S.
inline void watch() { esp_task_wdt_add(nullptr); }
inline void feed() { esp_task_wdt_reset(); }
// Before a watched task deletes itself (or the watchdog would fire for it).
inline void forget() { esp_task_wdt_delete(nullptr); }
}
