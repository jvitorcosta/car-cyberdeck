#pragma once
#include <cstddef>
#include <cstdint>
// A file from platformio.ini's board_build.embed_files, linked into flash. Its symbol is its path
// with '/' and '.' as '_': EMBED(data_greeting_pcm) declares data/greeting.pcm's start and end;
// EMBED_DATA / EMBED_SIZE read them. The declaration must be at namespace scope.
#define EMBED(sym)                                                                     \
    extern const uint8_t _binary_##sym##_start[] asm("_binary_" #sym "_start");      \
    extern const uint8_t _binary_##sym##_end[] asm("_binary_" #sym "_end")
#define EMBED_DATA(sym) (_binary_##sym##_start)
#define EMBED_SIZE(sym) ((size_t)(_binary_##sym##_end - _binary_##sym##_start))
