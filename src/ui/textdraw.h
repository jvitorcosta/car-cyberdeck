#pragma once
#include <TFT_eSPI.h>
#include <stddef.h>
#include "../util/glyphrun.h"
// Text with Portuguese/Latin-1 accents (pixel marks over font 2), Japanese/Chinese (Unifont
// glyphs from flash) and emoji (16 px Twemoji from flash, no shadow). Font 2 only: all dynamic
// text uses it.
namespace ui {
// Vertical offset of the 16-row Unifont cell vs font 2's (baseline row 12); tune on the panel.
constexpr int CJK_DY = 0;
// UTF-8 in font 2, optionally upper-cased (ASCII only), truncated with "..." to maxW; 1 px shadow.
// Datums: TL, TC, TR, ML, MC. Returns the drawn width.
int drawText(TFT_eSPI& t, const char* utf8, int x, int y, uint16_t fg,
             uint16_t shadow, uint8_t datum, int maxW, bool upper = false);
// Draws an already-decoded run (e.g. one wrapped lyric line). Returns its width.
int drawRun(TFT_eSPI& t, const glyphrun::Item* it, size_t n, int x, int y, uint16_t fg,
            uint16_t shadow, uint8_t datum);
// drawRun at an integer scale (2 = the companion's big lyric line): font 2 via setTextSize,
// accent marks, CJK glyphs and emoji pixel-doubled. Items keep their scale-1 widths.
int drawRunScaled(TFT_eSPI& t, const glyphrun::Item* it, size_t n, int x, int y, uint16_t fg,
                  uint16_t shadow, uint8_t datum, int scale);
// Width callbacks for glyphrun (ctx = TFT_eSPI*).
int asciiWidth2(char c, void* ctx);
int wideWidth(uint32_t cp, void* ctx);
int emojiWidth(uint32_t key, void* ctx);
}
