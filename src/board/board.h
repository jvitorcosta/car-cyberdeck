#pragma once
// Per-board hooks; build_src_filter compiles board/s3/ (the deck) or board/cyd/ (the companion).
namespace board {
void begin();      // right after Serial.begin(): board-specific early setup and boot log
bool sdMount();    // mounts the SD card as the global `SD` at "/sd"; false = no card, no cache
}
