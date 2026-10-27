#pragma once
// Per-board hooks; build_src_filter compiles board/s3/ (the black deck).
namespace board {
void begin();      // right after Serial.begin(): board-specific early setup and boot log
}
