#pragma once
// Per-board hooks; build_src_filter compiles board/s3/ (the deck) or board/cyd/ (the companion).
namespace board {
void begin();      // right after Serial.begin(): board-specific early setup and boot log
}
