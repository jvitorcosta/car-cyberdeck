# The Karaoke Look and the Menu's Bigger Buttons: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redraw the black deck's karaoke screen in the M1 look, and lay out the yellow Menu with
bigger lights and buttons. M1 is frameless and dark, uses a bold legible face, takes its colour
from the car's inside light, fits a line in up to 3 rows before cutting it, and has a half-size
walker.

**Architecture:**
- **Pure logic, host-tested** under `src/util/`:
  - `karaokepal`: the palette.
  - `glyphrun::wrapRows`: 3-row wrapping.
  - `lyricface`: the new GFX faces' metrics.
  - `karaokemode::parseLight`: reads the light parameter.
  - `keepalive`: the yellow deck's ask timing.
- **Device code** draws with it: `src/black/ui/karaokeview.cpp` and `src/yellow/ledpanel.cpp`.
- **The light colour** travels on the existing signed keep-alive as `light=N`.

**Tech Stack:** C++17, PlatformIO, TFT_eSPI 2.5.43 (GFX free fonts via `setFreeFont`), Unity host
tests (`pio test -e native`), Python 3 + Pillow (`tools/gfxfont.py`).

**Spec:** `docs/superpowers/specs/2026-11-09-karaoke-look-and-menu-design.md`

## Global Constraints

- **Branch** `feat/black-karaoke`, worktree `D:\dev\projects\spotify-pokemon-deck-health`.
- **Commits** are conventional, `type(scope): subject`, and end with
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- **Public repo:** no secrets, SSIDs, car data, LED addresses or personal paths.
- **Comments** match the files' style: plain sentences saying what a thing is and why.
- **Yellow deck heap is tight** (about 32 to 35 KB with WiFi and asks running). Add no new tasks,
  polls or buffers there.
- **The yellow deck's signed query buffer is 32 chars.** `on=1&light=-1` must fit.
- **Day/night on the black deck** is `theme::isNightActive()`, set from `daynight::isNight`
  (18:00 to 06:00).
- **Swatch indices** are ledlook `Colour`: RED 0, ORANGE 1, PINK 2, PURPLE 3, BLUE 4, GREEN 5,
  WHITE 6, YELLOW 7, CYAN 8. -1 means off, no link, or none yet.
- **Default theme** before any swatch, or for -1: WHITE (6).
- **Flashing:** the yellow deck over USB (COM17) is approved. The black deck (COM12) needs the
  user's OK each time.

## Review Focus

1. **CYAN (8)** has no termpal theme. It must look like blue, not the green default.
   Pinned in Task 2: `test_cyan_follows_blue`.
2. **A line of one unbreakable word wider than the screen** (a URL, "Aaaaaaah...") must hard-break
   across rows and get "..." on the last, never draw past the right edge.
   Pinned in Task 4: `test_rows_hard_break_long_word`.
3. **A `light` parameter that's missing, empty, non-numeric or out of range** ("12", "-5", "abc")
   must count as -1, not crash or pick a random colour.
   Pinned in Task 3: `test_parse_light_rejects_junk`.
4. **A colour change while karaoke is off** must not make the yellow deck ask the black deck.
   Pinned in Task 5: `test_light_change_ignored_while_off`.
5. **Accented Portuguese words** ("coração", "você") must draw with their accents in the new face.
   Pinned in Task 6: `test_latin1_compose`.

---

### Task 1: termpal moves to util/common so both decks build it

**Files:**
- Move: `src/util/yellow/termpal.h`, `src/util/yellow/termpal.cpp` → `src/util/common/`
- Modify: includes in `src/yellow/ledpanel.cpp`, `src/util/car/healthview.h`,
  `test/test_termpal/test_termpal.cpp`, and any other `grep -rn "yellow/termpal"` hit

**Interfaces:**
- Produces: `termpal::rgb(Role r, int theme, bool night) -> uint32_t`, unchanged, now at
  `src/util/common/termpal.h`.

- [ ] **Step 1:** `git mv` both files and fix every include. `grep -rn "termpal.h" src test` must
  show only `util/common/termpal.h` paths.
- [ ] **Step 2:** Run `pio test -e native -f test_termpal`, then `pio run -e ledpanel` and
  `pio run -e s3`.
  Expected: tests PASS, both builds SUCCESS.
- [ ] **Step 3:** Commit `refactor(common): termpal moves to util/common for both decks`.

### Task 2: karaokepal, the M1 palette

**Files:**
- Create: `src/util/black/karaokepal.h`, `src/util/black/karaokepal.cpp`
- Test: `test/test_karaokepal/test_karaokepal.cpp`

**Interfaces:**
- Consumes: `termpal::rgb(Role, int theme, bool night)` (Task 1).
- Produces:
  ```cpp
  namespace karaokepal {
  struct Pal { uint32_t bg, song, artist, barOn, barOff, lit, unsung, prev, next; };  // 0xRRGGBB
  constexpr int DEFAULT_SWATCH = 6;  // white
  uint32_t tint(uint32_t c, int pct, uint32_t base);  // per channel (c*pct + base*(100-pct) + 50) / 100
  Pal of(int swatch, bool night);  // swatch -1 or out of range: DEFAULT_SWATCH; 8 (cyan): 4 (blue)
  }
  ```

The values are the spec's table. Let T/M/D be `termpal::rgb(TEXT/MID/DIM, theme, night)`, and
`grey` be 0x8A929C by day and 0x5A6068 at night. Then:

| Field | Day | Night |
|---|---|---|
| `bg` | `tint(D, 10, 0x0E1218)` | `tint(D, 7, 0x030406)` |
| `song` | T | T |
| `artist` | M | M |
| `barOn` | T | T |
| `barOff` | `tint(D, 30, bg)` | `tint(D, 30, bg)` |
| `lit` | 0xFFFFFF | 0xEDE6D6 |
| `unsung` | `tint(D, 45, grey)` | `tint(D, 45, grey)` |
| `prev` | `tint(D, 55, grey)` | `tint(D, 55, grey)` |
| `next` | `tint(M, 70, grey)` | `tint(M, 70, grey)` |

- [ ] **Step 1: Write the failing tests**
  ```cpp
  void test_tint_mixes_per_channel() { TEST_ASSERT_EQUAL_HEX32(0x231A16, karaokepal::tint(0xDC6400, 10, 0x0E1218)); }
  void test_orange_day() { auto p = karaokepal::of(1, false);
      TEST_ASSERT_EQUAL_HEX32(0xFFAA64, p.song); TEST_ASSERT_EQUAL_HEX32(0xF57814, p.artist);
      TEST_ASSERT_EQUAL_HEX32(0x231A16, p.bg); TEST_ASSERT_EQUAL_HEX32(0xFFFFFF, p.lit); }
  void test_orange_night() { auto p = karaokepal::of(1, true);
      TEST_ASSERT_EQUAL_HEX32(0xFF4600, p.song); TEST_ASSERT_EQUAL_HEX32(0xEDE6D6, p.lit); }
  void test_none_is_white() { TEST_ASSERT_EQUAL_HEX32(0xD2D2C8, karaokepal::of(-1, false).song);
      TEST_ASSERT_EQUAL_HEX32(0xD2D2C8, karaokepal::of(42, false).song); }
  void test_cyan_follows_blue() { TEST_ASSERT_EQUAL_HEX32(karaokepal::of(4, false).song, karaokepal::of(8, false).song); }
  void test_lit_is_brightest() {  // for every swatch, day and night: lit's luminance > unsung, prev, next
      ... loop swatch -1..8, night 0..1, compare termpal::contrastOnBlack(lit) > contrastOnBlack(each) }
  ```
- [ ] **Step 2:** Run `pio test -e native -f test_karaokepal`.
  Expected: FAIL, does not compile (no karaokepal.h).
- [ ] **Step 3:** Implement `karaokepal.h/.cpp` as above.
- [ ] **Step 4:** Run the same command.
  Expected: 6/6 PASS.
- [ ] **Step 5:** Commit `feat(black): the karaoke palette from the car's inside light`.

### Task 3: the black deck takes `light=N` on /karaoke

**Files:**
- Modify: `src/util/black/karaokemode.h/.cpp`, `src/black/core/karaoke.h/.cpp`,
  `src/black/net/remote.cpp` (`handleKaraoke`, around line 183)
- Test: `test/test_karaokemode/` (add tests)

**Interfaces:**
- Produces:
  - `int karaokemode::parseLight(const char* s)`: 0..8 for a plain decimal in range, else -1.
    nullptr and "" also give -1.
  - `void karaoke::keepAlive(int light)` replaces `keepAlive()` and stores the light under the
    portMUX.
  - `int karaoke::light()` returns the last light kept: -1 before any, and it stays after `stop()`.
- Consumes: nothing new.

- [ ] **Step 1: Write the failing tests**
  ```cpp
  void test_parse_light_reads_swatches() { TEST_ASSERT_EQUAL_INT(0, karaokemode::parseLight("0"));
      TEST_ASSERT_EQUAL_INT(8, karaokemode::parseLight("8")); TEST_ASSERT_EQUAL_INT(-1, karaokemode::parseLight("-1")); }
  void test_parse_light_rejects_junk() { for (const char* s : {"", "12", "9", "-5", "abc", "3x", " 3"})
      TEST_ASSERT_EQUAL_INT(-1, karaokemode::parseLight(s)); TEST_ASSERT_EQUAL_INT(-1, karaokemode::parseLight(nullptr)); }
  ```
- [ ] **Step 2:** Run `pio test -e native -f test_karaokemode`.
  Expected: FAIL, `parseLight` is not a member.
- [ ] **Step 3:** Implement `parseLight`. Then:
  - `handleKaraoke` on `on=1` calls `karaoke::keepAlive(karaokemode::parseLight(arg("light")))`.
    A missing arg gives -1.
  - `light` must be among the signed parameters that `reqauth` checks, like `on`. Read
    `reqauth::pass` and add it the same way `on` is covered.
- [ ] **Step 4:** Run the native test, then `pio run -e s3`.
  Expected: PASS, SUCCESS.
- [ ] **Step 5:** Commit `feat(black): /karaoke takes the inside light's colour`.

### Task 4: glyphrun wraps up to 3 rows

**Files:**
- Modify: `src/util/black/glyphrun.h/.cpp`
- Test: `test/test_glyphrun/test_glyphrun.cpp` (add tests)

**Interfaces:**
- Produces:
  ```cpp
  struct Rows { size_t start[3], end[3]; int n; bool ellipsis; };  // row i = [start[i], end[i]); "..." after the last row
  Rows wrapRows(const Item* it, size_t n, int maxW, int ellipsisW, int maxRows);  // maxRows 1..3
  ```
  The break rules are wrapTwo's:
  - Break at the last fitting space, or between characters next to a Wide or Emoji item, else hard.
  - Leading spaces of a new row are skipped.
  - When the text runs past the last row, that row is cut so the row plus ellipsisW fits maxW, and
    `ellipsis` is set.

  `wrapTwo` stays for `screen_now.cpp`.

- [ ] **Step 1: Write the failing tests.** Use the file's fake fonts: ASCII 6 px, so maxW 60 holds
  10 characters.
  ```cpp
  void test_rows_one_row_when_it_fits()   // "short line", 60 → n 1, ellipsis false
  void test_rows_three_rows_by_spaces()   // "aaaa bbbb cccc dddd eeee ffff", 60, 3 → n 3, row texts "aaaa bbbb", "cccc dddd", "eeee ffff", no ellipsis
  void test_rows_ellipsis_on_last_row()   // the same text + " gggg", 3 rows → ellipsis true, width(row 2) + 18 <= 60
  void test_rows_hard_break_long_word()   // 35 'a's, 60, 3 → rows 10, 10, and the third cut so width + 18 <= 60, ellipsis true
  void test_rows_break_beside_cjk()       // "ab" + 6 kana (16 px each), 60 → no row wider than 60, every item in some row
  ```
- [ ] **Step 2:** Run `pio test -e native -f test_glyphrun`.
  Expected: FAIL, `wrapRows` is not a member.
- [ ] **Step 3:** Implement `wrapRows`, a loop over rows reusing wrapTwo's break helper.
- [ ] **Step 4:** Run the same command.
  Expected: all PASS.
- [ ] **Step 5:** Commit `feat(black): wrap a lyric line to up to three rows`.

### Task 5: the yellow deck sends its inside light, at once on a change

**Files:**
- Create: `src/util/yellow/keepalive.h`, `src/util/yellow/keepalive.cpp`
- Modify: `src/yellow/ledpanel.cpp` (the `KeepAlive` struct, `keepAlive`, `karaokeGet`, `deckTask`)
- Test: `test/test_keepalive/test_keepalive.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace keepalive {
  struct State { bool asked = false; uint32_t nextAt = 0; int sentLight = -2; };  // -2: none sent
  enum class Ask : uint8_t { None, On, Off };
  // what to send now: On when wanted and (not yet asked, or nextAt has passed, or light != sentLight); Off once when it stops being wanted
  Ask due(const State& s, bool want, uint32_t nowMs, int light);
  }
  ```
  ledpanel keeps its rc logging and MUSIC_KEEP_MS / MUSIC_RETRY_MS pacing. Music mode's keep-alive
  passes `light = -1` always, so it never asks early.
- Consumes: the black deck's `light` parameter (Task 3).

- [ ] **Step 1: Write the failing tests**
  ```cpp
  void test_first_want_asks_on()            // State{}, want, 0, 1 → On
  void test_waits_until_next()              // asked, nextAt 10000, now 5000, light == sentLight → None
  void test_light_change_asks_early()       // asked, nextAt 10000, now 5000, sentLight 1, light 4 → On
  void test_light_change_ignored_while_off()// not asked, want false, light changes → None
  void test_stop_asks_off_once()            // asked, want false → Off
  ```
- [ ] **Step 2:** Run `pio test -e native -f test_keepalive`.
  Expected: FAIL, no keepalive.h.
- [ ] **Step 3:** Implement `due`. Then wire it into ledpanel:
  - **The light value.** A `static volatile int insideSwatch = -1;` that the loop sets whenever the
    INSIDE light's shown look changes. Use `shownLook(0)`: its `swatch` when `lit` and linked,
    else -1. Set it where `drawLightsBand` already learns of a look or link change, so there's no
    new hook.
  - **The request.** `karaokeGet` adds `{"light", buf}` to the signed params; `buf` holds
    `insideSwatch` as decimal, -1 included.
  - **The bookkeeping.** `keepAlive` records `sentLight` on each On.
- [ ] **Step 4:** Run the native test, then `pio run -e ledpanel`.
  Expected: PASS, SUCCESS.
- [ ] **Step 5:** Commit `feat(yellow): the karaoke ask carries the inside light, sent on a change`.

### Task 6: the bold face for the karaoke (lyricface)

**Files:**
- Modify: `tools/gfxfont.py` (two options: `--ns NAME`, whose types are `NAME::Glyph` and
  `NAME::Font`, default `termfont`; and `--licence TEXT`)
- Create:
  - `tools/fonts/AtkinsonHyperlegible-Bold.ttf` and `tools/fonts/AtkinsonHyperlegible-OFL.txt`,
    downloaded from the google/fonts GitHub repo, `ofl/atkinsonhyperlegible/`
  - `src/black/fonts/atkb_22.h`, `atkb_18.h`, `atkb_14.h`, `atkb_12.h`
  - `src/util/black/lyricface.h/.cpp`
- Modify: `src/util/common/text.h/.cpp` (compose), `src/black/ui/textdraw.h/.cpp` (a face-aware draw)
- Test: `test/test_lyricface/test_lyricface.cpp`, `test/test_text/` (add a test)

**Interfaces:**
- Produces:
  ```cpp
  namespace lyricface {
  struct Glyph { uint32_t bitmapOffset; uint8_t width, height, xAdvance; int8_t xOffset, yOffset; };  // = GFXglyph
  struct Font { const uint8_t* bitmap; const Glyph* glyph; uint16_t first, last; uint8_t yAdvance; };  // = GFXfont
  enum class Face : uint8_t { SUNG, SUNG_SMALL, LINE, SMALL };  // 22, 18, 14, 12 px
  const Font& font(Face f);
  int advance(Face f, uint8_t latin1);  // 0 for a code outside the font
  int lineHeight(Face f);               // yAdvance
  }
  uint8_t txt::latin1(char base, txt::Mark m);  // the Latin-1 letter of base + mark ('c', Cedilla → 0xE7), 0 when there's none
  // textdraw: draws a decoded run in a GFX face; Ascii items with a mark draw their composed Latin-1
  // glyph (or base + font-2 mark when latin1 is 0); Wide/Emoji draw at scale 1, centred on the row.
  int ui::drawRunFace(TFT_eSPI& t, const glyphrun::Item* it, size_t n, int x, int y, uint16_t fg,
                      lyricface::Face f);  // y = the row's top; returns the width
  int ui::faceWidth(char c, void* ctx);    // glyphrun AsciiWidthFn; ctx = (void*)(intptr_t)Face
  ```
  The structs must match GFXglyph and GFXfont byte for byte, as `termfont`'s do. The device casts
  with `reinterpret_cast<const GFXfont*>` for `setFreeFont`. The black build needs `-D LOAD_GFXFF=1`
  in `[env:s3]` build_flags.

- [ ] **Step 1:** Download the TTF and OFL text from the google/fonts repo (raw.githubusercontent.com). Generate the
  four headers:
  `python tools/gfxfont.py tools/fonts/AtkinsonHyperlegible-Bold.ttf 22 src/black/fonts/atkb_22.h ATKB_22 --ns lyricface --licence "SIL OFL 1.1, see tools/fonts/AtkinsonHyperlegible-OFL.txt"`,
  then the same for 18, 14 and 12. Running the script with no options must still regenerate
  `vt323_24.h` byte-identical: check with `git diff --stat`.
- [ ] **Step 2: Write the failing tests**
  ```cpp
  void test_faces_shrink()     // advance(SUNG,'M') > advance(SUNG_SMALL,'M') > advance(LINE,'M') > advance(SMALL,'M') > 0
  void test_latin1_covered()   // advance(SUNG, 0xE7) > 0 and advance(SUNG, 0xE3) > 0   (ç, ã)
  void test_line_heights()     // lineHeight(SUNG) in 22..30, lineHeight(SUNG_SMALL) < lineHeight(SUNG)
  // test_text:
  void test_latin1_compose()   // latin1('c',Cedilla)==0xE7, latin1('a',Tilde)==0xE3, latin1('e',Circumflex)==0xEA, latin1('x',Acute)==0
  ```
- [ ] **Step 3:** Run `pio test -e native -f test_lyricface -f test_text`.
  Expected: FAIL, no lyricface.h and no `txt::latin1`.
- [ ] **Step 4:** Implement `lyricface.cpp` (it includes the four headers), `txt::latin1`,
  `drawRunFace` and `faceWidth`, and add the build flag.
- [ ] **Step 5:** Run the native tests, then `pio run -e s3`.
  Expected: PASS, SUCCESS.
- [ ] **Step 6:** Commit `feat(black): Atkinson Hyperlegible Bold for the karaoke`.

### Task 7: the karaoke view in the M1 look

**Files:**
- Modify:
  - `src/black/ui/karaokeview.h/.cpp` (a rewrite of the drawing; the public functions keep their names)
  - `src/black/ui/screen_now.h/.cpp` (`blitWalker` gains `bool half = false`)
  - `src/black/main.cpp` (`drawKaraokeView`: the whole-redraw triggers)

**Interfaces:**
- Consumes: `karaokepal::of` (Task 2), `karaoke::light()` (Task 3), `glyphrun::wrapRows`
  (Task 4), `lyricface`, `ui::drawRunFace` and `ui::faceWidth` (Task 6).
- Produces: `void ui::blitWalker(TFT_eSPI&, int cx, int feetY, uint32_t animMs, int step,
  uint16_t fill, walkrect::Rect& prev, bool half = false)`. With `half`, every second pixel of
  every second row is drawn: a 64x32 frame becomes 32x16. `walkerWidth()` callers that pass
  `half` halve it.

**Layout** (320x240, the karaoke clip still applied; tune on the panel):
- **Song:** LINE face at y 6, colour `song`, upper-case.
- **Artist:** SMALL face at y 24, colour `artist`.
- **Bar:** x 12..308, y 42, 4 px, `barOn` over `barOff`.
- **Line just sung:** LINE face, top at 52, colour `prev`, one row, cut with "...".
- **Sung line:**
  - First try SUNG: `wrapRows(..., maxW 296, ..., 3)`, row 1's top at ROW1_Y 96, rows
    `lineHeight` apart.
  - When that sets `ellipsis`, decode again with SUNG_SMALL widths and wrap again. Keep the "..."
    only if it is still set.
  - Unsung colour `unsung`, swept to `lit`.
- **Next line:** LINE face, one row, colour `next`. Its top is 12 px under the last row drawn,
  no higher than 186.
- **Walker:** half size, feet on ROW1_Y minus the hop. Its x comes from `karaokeWalkerOffset`
  across all rows, mapped onto row 1 (the spec's "our call").
- **Background:** the whole screen in `bg`. No dialogue box, no top band.
- **Messages:** lyricmsg text in SUNG, colour `lit`, through the same 3-row fit.

**When the whole screen redraws:** `main.cpp` already does it on a screen change. It must also do
it when `karaoke::light()` or `theme::isNightActive()` differs from what was drawn: keep both in
`drawKaraokeView`'s statics. `litBand`'s clip math stays as is, only the row geometry changes.

Each `0xRRGGBB` becomes 565 with `rgb565::hex` at draw time.

- [ ] **Step 1:** Implement the above. Remove the now-unused `dialogueBox` call, `mix` and
  `dimTone` from karaokeview.
- [ ] **Step 2:** Run `pio test -e native` (the whole suite) and `pio run -e s3`.
  Expected: all PASS, SUCCESS.
- [ ] **Step 3:** Commit `feat(black): the karaoke in the night drive look, tinted by the inside light`.
- [ ] **Step 4:** On-panel check. Ask the user before flashing COM12. Check:
  - Day and night: set via the `daynight` override if one exists, else by clock.
  - Two light colours.
  - A long line that needs 3 rows.
  - The camera band.

### Task 8: the yellow Menu with bigger lights and buttons

**Files:**
- Modify: `src/util/yellow/deckmenu.h/.cpp`, `src/yellow/ui/termlayout.h`, `src/yellow/ledpanel.cpp`
  (`drawClock`, `drawTripTime`, `drawLightsBand`, `drawPrompt` positions, `drawDash`, the items' label y)
- Test: `test/test_deckmenu/test_deckmenu.cpp`, `test/test_termfont/test_termfont.cpp`
  (update `test_menu_bands_stack`, `test_menu_ids_and_hits`, `test_dash_clock_and_trip_fit`,
  `test_dash_lights_fit`, `test_dash_prompt_fits` and `test_menu_items_fit` to the new numbers)

**Values:**
- **deckmenu rects:**
  - `HEADER{0,0,320,22}`
  - `CLOCK_BAND{0,22,320,31}`
  - `LIGHTS_BAND{0,53,320,49}`
  - `PROMPT_BAND{0,102,320,24}`
  - Menu items `mk(4 + i*106, 130, 100, 106)`
- **termlayout:**
  - Rules: `CLOCK_RULE_Y 52`, `LIGHTS_RULE_Y 101`.
  - Clock row: the clock and trip time in TITLE, with their caps vertically centred between rows 21
    and 52. The date and TRIP are in SMALL, bottom-aligned with the TITLE caps. The date sits
    `CLOCK_GAP 8` after the clock. TRIP sits `CLOCK_GAP` before the trip time.
  - Lights band:
    - The swatch block is `LIGHT_BLOCK_W 40` x `LIGHT_BLOCK_H 34`, at `lightX(i)` and y 60.
    - The label (`INSIDE` / `OUTSIDE`, SMALL MID) is at x `lightX(i) + 46`, caps top 61.
    - The value is in the face `LIGHT_VALUE_FACE`, caps bottom 94. It's TITLE if
      `width(TITLE, "PURPLE 100%") <= 159 - 46 - 8`, else BODY. Decide by running the test, then
      hard-code the face with a comment giving the reason.
  - `PROMPT_BASE` is centred in the prompt band.
  - Item labels: BODY, caps vertically centred in the 106-row item. The tags stay bottom right.
- **States:**
  - Off: the block is drawn as a DIM outline and the value is OFF in DIM.
  - No link: no block, and "NO LINK" in ALERT at the value's place.
- **Text:** `LIGHT_LABELS` becomes `{"INSIDE", "OUTSIDE"}`.

- [ ] **Step 1: Update the tests first** to the values above, including:
  ```cpp
  void test_menu_items_are_tall()      // rect(Menu, M_ITEM0+i).h == 106, .y == 130, and hit(Menu, 54, 230) == M_ITEM0
  void test_lights_band_is_no_hit()    // hit(Menu, 60, 75) == -1
  void test_dash_lights_fit()          // the widest value ("PURPLE 100%") in LIGHT_VALUE_FACE fits 159-46-8 px; "OUTSIDE" fits beside the block
  ```
- [ ] **Step 2:** Run `pio test -e native -f test_deckmenu -f test_termfont`.
  Expected: FAIL on the new numbers.
- [ ] **Step 3:** Change deckmenu, termlayout and ledpanel to match.
- [ ] **Step 4:** Run the same command, then `pio run -e ledpanel`.
  Expected: PASS, SUCCESS.
- [ ] **Step 5:** Commit `feat(yellow): bigger lights and buttons on the Menu`.
- [ ] **Step 6:** Flash the yellow deck over USB (COM17, approved):
  `pio run -e ledpanel -t upload --upload-port COM17`. Look at the Menu and tap each item. Check the
  `[mem]` line's heap against the 32 to 35 KB baseline.

### Task 9: docs

**Files:** `CHANGELOG.md`, `docs/ARCHITECTURE.md` (the karaoke section), `docs/yellow-deck.md`
(the Menu), `README.md` if it describes either screen.

- [ ] **Step 1:** Describe the M1 look, `light=N`, and the Menu's new layout in each file's style.
  Then `pio test -e native`.
  Expected: all PASS.
- [ ] **Step 2:** Commit `docs: the karaoke's night drive look and the Menu's bigger buttons`.
