# Pokéface v2 (Emerald Battle) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rework Pokéface into Emerald's battle screen: 21 Gen 3-style Pokémon (a day/night pair per weekday plus a reward legendary), drawn at 2× on a light grass field, with Emerald info boxes, HP/EXP bars and the game's pixel font.

**Architecture:** `tools/gen_pokeface.py` is rewritten. From one pinned commit of rh-hideout/pokeemerald-expansion, it builds:
- one 128×64 sheet per Pokémon (2 frames, palette applied, mirrored);
- three BMFont bitmap fonts (1×, 2×, and 4× digits) from Emerald's font sheet;
- a `gen.xml` resource file.

Everything it builds goes into the git-ignored `garmin/pokeface/resources/gen/`. `Partner` becomes pure weekday/day-night/reward logic. `Sprite` draws a sheet's frame at 2× with a point-filtered transform. A new `Battle` module draws the background, platform and boxes. `PokefaceView` keeps v1's once-a-minute readings, timer cap and data, and lays them out as layout B.

**Tech Stack:** Monkey C (CIQ 5.2 on fr255: `drawBitmap2` with `AffineTransform` and `FILTER_MODE_POINT`, custom BMFont fonts), Python 3 + Pillow, `unittest`.

**Spec:** `docs/superpowers/specs/2026-10-23-pokeface-emerald-design.md` (on top of `docs/superpowers/specs/2026-10-23-pokeface-watch-face-design.md`)

## Global Constraints

- **Branch:** `feat/pokeface`. **Products:** fr255 and fr255m. **Watch-face memory:** 131072 bytes, measured in the simulator.
- **Source:** every download comes from `https://raw.githubusercontent.com/rh-hideout/pokeemerald-expansion/e8914bd0ae80db0af135056f37e3aad5664c1be9/`. That covers `graphics/pokemon/<folder>/anim_front.png` and `normal.pal` for each of the 21 Pokémon, plus `graphics/fonts/latin_normal.png`, `src/fonts.c` and `charmap.txt`: 45 files.
- **Approval:** João approves that list in chat before the first fetch. **The font is the game's own font sheet from that repo, not a fan TTF.** The FontStruct "Pokémon Emerald Pro" recreation needs a FontStruct account to download, so ask João to confirm the swap at the same time.
- **Git-ignored:** `pokefont-src/` (fetched files) and `garmin/pokeface/resources/gen/`. Remove the v1 ignores `pokeface-sprites/` and `garmin/pokeface/resources/sprites/`, and delete those folders locally.
- **Species order:** this is the index used everywhere (tool, `gen.xml` ids `Mon0`…`Mon20`, `Partner.NAMES`). For each weekday Monday→Sunday it goes day, night, reward:
  - 0 Blaziken, 1 Sceptile, 2 Latias
  - 3 Ho-Oh (folder `ho_oh`), 4 Lugia, 5 Suicune
  - 6 Rayquaza, 7 Deoxys, 8 Latios
  - 9 Cresselia, 10 Darkrai, 11 Jirachi
  - 12 Arceus, 13 Giratina, 14 Dialga
  - 15 Solgaleo, 16 Lunala, 17 Necrozma
  - 18 Xerneas, 19 Yveltal, 20 Zygarde
- **Day and night:** day is `rise <= now < set`. Without sun times it falls back to 06:00 ≤ hour < 18:00.
- **Reward:** run finished today **or** steps ≥ goal (goal > 0). It holds until midnight, day or night.
- **HP bar (Body Battery):** green if >50, yellow if 21–50, red if ≤20, empty grey if unknown.
- **Text sizes:** the time uses the 4× digits font. The Pokémon's name and the four bottom values (heart rate, temperature, calories, distance) use 2×. Lv, BB, the step number, the date/notifications/phone/battery line and the weather/sun line use 1×.
- **Scaling:** sprites draw at 2× with point filtering only, so no blur.
- **Kept from v1:** readings once a minute and on show/wake, `MAX_FRAMES` cap, timer stop on hide/sleep, `--` for missing values, `finishedToday`, `Fmt.*` and `Stats.*`.
- **Commits:** `type(garmin): subject`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- **Commands:** reuse the v1 plan's workspace scripts, copied into this plan's workspace: `ciqtest.ps1` (unit tests, pass = `PASSED (passed=N, failed=0, errors=0)`), `build.ps1 [-Run]`, `shot.ps1 -Out <png>`. Python: `python tools/test_gen_pokeface.py`.

## Review Focus

1. **Midnight → sunrise:** the new weekday's *night* Pokémon must show, not the old day's night one. Test: `testWhoAfterMidnight` (Task 2).
2. **No sun times** (no weather position): day/night falls back to 06:00–18:00 instead of always night. Test: `testIsDayFallback` (Task 2).
3. **Body Battery unknown or at the thresholds** (20/21/50/51): the HP colour must be right, and unknown must not crash. Test: `testHpColour` (Task 2).
4. **A source sheet with only one 64×64 frame:** the tool still writes a valid 128×64 sheet with the frame duplicated. Test: `test_single_frame_sheet` (Task 1).
5. **Characters the Emerald charmap lacks** (`°`): the font still contains them, so temperatures render. Test: `test_degree_glyph_synthesised` (Task 1).

---

### Task 1: Sprite and font tool v2

**Files:**
- Rewrite: `tools/gen_pokeface.py`, `tools/test_gen_pokeface.py`
- Modify: `.gitignore`

**Interfaces:**
- Produces, Python:
  - `BASE` (the pinned URL).
  - `SPECIES`: 21 `(folder, display_name)` tuples in the Global order.
  - `read_jasc(text) -> list[tuple[int,int,int]]`.
  - `frames(sheet: Image) -> list[Image]`: from a mode-P image, a 64×128 stack gives 2 frames and a 64×64 gives that frame twice.
  - `colourise(frame: Image, pal) -> Image`: RGBA; palette index 0 becomes transparent, other colours are snapped to the 64 levels (reuse v1 `snap`).
  - `sheet(frames) -> Image`: 128×64, frame 0 left and frame 1 right, each mirrored left↔right.
  - `read_charmap(text) -> dict[str,int]`: from lines like `'A' = BB`.
  - `read_widths(c_source) -> list[int]`: the `gFontNormalLatinGlyphWidths` array.
  - `glyph(fontsheet: Image, code: int, width: int) -> Image`: from a 16×16 cell grid, cell `code` at `((code % 16) * 16, (code // 16) * 16)`, cropped to `width`; foreground pixels opaque white, the rest transparent.
  - `GLYPHS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz :./-%°"`.
  - `bmfont(glyphs: dict[str,Image], scale: int, name: str) -> tuple[str, Image]`: BMFont text (`info`/`common`/`page`/`chars`/`char` lines) and its page image, glyphs scaled with `NEAREST`.
  - `degree(height: int) -> Image`: a synthesised 3×3 ring for `°`.
  - `gen_xml() -> str`.
- Produces, on disk (git-ignored), in `garmin/pokeface/resources/gen/`:
  - `mon0.png`…`mon20.png`;
  - `emerald1.fnt/.png`, `emerald2.fnt/.png`, `emerald4.fnt/.png` (the 4× font holds only `0123456789:`);
  - `gen.xml`, holding `<drawables>` with `<bitmap id="Mon<i>" filename="mon<i>.png" dithering="none"/>` and `<fonts>` with `<font id="Emerald1" filename="emerald1.fnt" antialias="false"/>`, plus `Emerald2` and `EmeraldTime`.

- [ ] **Step 1: Ask João to approve the 45 files** (Global Constraints) and the game font in place of a fan TTF. Don't fetch until he says yes.
- [ ] **Step 2: Write the failing tests** (`unittest`, synthetic images only):
  - `test_read_jasc`: `"JASC-PAL\n0100\n2\n0 0 0\n248 64 32\n"` → `[(0,0,0),(248,64,32)]`.
  - `test_two_frame_stack`: a 64×128 P image with index 1 at (0,0) and index 2 at (0,64) gives 2 frames whose (0,0) are 1 and 2.
  - `test_single_frame_sheet`: a 64×64 source gives 2 identical frames, and `sheet()` gives 128×64.
  - `test_colourise_index0_transparent`: an index 0 pixel has alpha 0; index 1 = (248,64,32) becomes (255,85,0,255).
  - `test_sheet_mirrors`: a frame with an opaque pixel at (0,10) appears at (63,10) in the sheet.
  - `test_read_charmap`: `"'A' = BB\n'0' = A1\n"` → `{'A':0xBB,'0':0xA1}`.
  - `test_read_widths`: `"const u8 gFontNormalLatinGlyphWidths[] = {\n 3, 6,\n 5 };"` → `[3,6,5]`.
  - `test_glyph_cut`: a 256×256 sheet with a foreground pixel in cell 0xBB at offset (1,2) gives a `glyph(…, 0xBB, 6)` 6 wide with (1,2) opaque.
  - `test_degree_glyph_synthesised`: `bmfont({'°': degree(10)}, 1, "t")[0]` contains `char id=176`.
  - `test_bmfont_scale`: a 2×3 glyph at scale 2 gives `width=4 height=6`, and `xadvance` = width·scale + scale.
  - `test_gen_xml`: has 21 `<bitmap ` and 3 `<font `, with `Mon0`, `Mon20` and `EmeraldTime`.
- [ ] **Step 3: Run** `python tools/test_gen_pokeface.py`. Expected: FAIL/ERROR (new names missing).
- [ ] **Step 4: Implement it.**
  - Drop the v1 B/W functions it no longer uses, but keep `snap`.
  - `main()` fetches into `pokefont-src/` (write to `.tmp`, then rename; this fixes v1's deferred minor) and builds everything.
  - It prints, for the font sheet, the colour counts of cell 'A' so the foreground colour can be checked.
  - The module docstring says what, where from, that nothing is committed, and how to run it.
- [ ] **Step 5: Run the tests.** Expected: OK.
- [ ] **Step 6: Run `python tools/gen_pokeface.py`.** Expected:
  - 21 PNGs of 128×64 and 3 fonts.
  - Open `emerald2.png` and two of the mon sheets and look at them: letters readable, Pokémon facing right, and no stray background colour (if there is one, palette index 0 wasn't transparent).
  - `git status` shows none of them.
  - If the foreground colour or widths array name differs from the brief, rule and ledger it.
- [ ] **Step 7: Commit** `feat(garmin): Pokéface v2 tool builds Emerald sprites and font`

### Task 2: Partner v2 and the 2× sprite

**Files:**
- Modify: `garmin/pokeface/source/Partner.mc`, `garmin/pokeface/source/Tests.mc`, `garmin/pokeface/source/Sprite.mc` (rewrite), `garmin/pokeface/source/PokefaceView.mc` (decide/draw the partner only), `garmin/pokeface/monkey.jungle` (`resources;resources/gen`)

**Interfaces:**
- Consumes: `Mon0`…`Mon20` and `FRAMES` = 2 (Task 1).
- Produces, `module Partner`:
  - Keep `earned`, `finishedToday`, `keepAnimating`/`MAX_FRAMES`, `barFraction`, `dayKey`.
  - Remove the v1 enum, `asleep`, `pick`, `megaFor`, `flameRadius`, and their tests.
  - `const NAMES` = the 21 display names in caps (`"HO-OH"` for 3).
  - `function row(dayOfWeek as Number) as Number`: CIQ `day_of_week` (1 = Sunday … 7 = Saturday) to 0 = Monday … 6 = Sunday.
  - `function isDay(now as Number, hour as Number, rise as Number or Null, set as Number or Null) as Boolean`.
  - `function who(dayOfWeek as Number, day as Boolean, earned as Boolean) as Number`: `row * 3 + (earned ? 2 : (day ? 0 : 1))`.
  - `enum { HP_NONE, HP_GREEN, HP_YELLOW, HP_RED }` and `function hpColour(bb as Number or Null) as Number`.
- Produces, `class Sprite`:
  - `function show(species as Number) as Void`: loads `Mon<species>` when it changes.
  - `function draw(dc, x as Number, y as Number, frame as Number) as Void`: top-left `(x, y)` at 128×128, using `dc.drawBitmap2(x, y, bmp, {:bitmapX => (frame % 2) * 64, :bitmapY => 0, :bitmapWidth => 64, :bitmapHeight => 64, :transform => t, :filterMode => Graphics.FILTER_MODE_POINT})`, where `t` = `new Graphics.AffineTransform()` scaled 2×.

- [ ] **Step 1: Write the failing tests**:
  - `testWho`: `who(2, true, false) == 0` (Monday day Blaziken), `who(2, false, false) == 1`, `who(2, true, true) == 2`, `who(1, true, false) == 18` (Sunday Xerneas), `who(7, false, false) == 16` (Saturday Lunala).
  - `testWhoAfterMidnight`: Tuesday 02:00 before sunrise → `who(3, isDay(now, 2, rise, set), false) == 4` (Lugia), with `rise` and `set` later today.
  - `testIsDayFallback`: `isDay(0, 12, null, null)`, `!isDay(0, 19, null, null)`, `!isDay(0, 5, null, null)`, `isDay(0, 6, null, null)`.
  - `testIsDaySun`: with `rise=1000, set=5000`: `isDay(3000,…)` is true; `isDay(500,…)` and `isDay(5000,…)` are false.
  - `testHpColour`: 51 → GREEN, 50 → YELLOW, 21 → YELLOW, 20 → RED, 0 → RED, null → NONE.
  - `testNames`: `NAMES.size() == 21`, `NAMES[0].equals("BLAZIKEN")`, `NAMES[3].equals("HO-OH")`, `NAMES[20].equals("ZYGARDE")`.
- [ ] **Step 2: Run `ciqtest.ps1`.** Expected: a compile error or FAIL.
- [ ] **Step 3: Implement `Partner` v2 and `Sprite`. In the view:**
  - `decide()` uses `who(now.day_of_week, isDay(...), earned)`, with the sun times from `refresh()`.
  - Drop `_asleep`; the animation runs day or night.
  - Draw the sprite at (14, 86).
  - Remove the "z Z" and flame code.
- [ ] **Step 4: Run `ciqtest.ps1`.** Expected: all PASSED.
- [ ] **Step 5: Run `build.ps1 -Run`, then `shot.ps1`.** Expected:
  - Today's Pokémon is drawn 128 px tall and crisp (no blur), facing right.
  - Two shots 250 ms apart differ (2-frame idle).
  - Memory in the simulator status bar is under 124 KB.
- [ ] **Step 6: Commit** `feat(garmin): Pokéface v2 weekday pairs and rewards at 2x`

### Task 3: The Emerald battle layout

**Files:**
- Create: `garmin/pokeface/source/Battle.mc`
- Modify: `garmin/pokeface/source/PokefaceView.mc` (`onUpdate` layout), `garmin/pokeface/source/Icons.mc` (only if a colour needs to suit the light background)

**Interfaces:**
- Consumes: `Partner.hpColour/barFraction/NAMES`, `Sprite` (Task 2), and fonts `Rez.Fonts.Emerald1`/`Emerald2`/`EmeraldTime` (Task 1).
- Produces, `module Battle`:
  - `function field(dc, night as Boolean) as Void`: sky and grass. The day colours come from Emerald's tall-grass battle look, snapped to the 64 colours; night uses dark blues and greens.
  - `function platform(dc, cx, cy, night as Boolean) as Void`: the oval under the Pokémon.
  - `function infoBox(dc, x, y, w, h) as Void`: cream fill, 2 px dark border, Emerald's slanted corner.
  - `function bar(dc, x, y, w, frac as Float, colour as Number) as Void`: the HP/EXP bar track and fill.
  - `function menuBox(dc, x, y, w, h) as Void`: the dark-blue box with a light inner border.
  - `function text(dc, x, y, font, s as String, colour, shadow, justify) as Void`: draws the shadow 1 px (or the font's scale) down-right, then the text. That is Emerald's drop shadow.

- [ ] **Step 1: Load the three fonts once** in `initialize`. Lay out `onUpdate` per spec layout B, starting from:
  - time centre (130, 40) in EmeraldTime;
  - 1× lines at y 70 (date · notifications · phone · battery) and y 84 (weather icon · high/low · next sun);
  - sprite top-left (14, 86) with the platform under it at (78, 206);
  - info box x 138–244, y 112–188, holding the name at 2×, `Lv<day>` at 1×, the HP bar (hpColour; grey when unknown), `BB nn`, the EXP bar (`barFraction`, blue) and the step count at 1×;
  - the menu box at the bottom with the four 2× values.
- [ ] **Step 2: Run `build.ps1 -Run` and `shot.ps1`.** Expected: nothing clipped by the round edge, no text overlapping the sprite or other text, and the Pokémon still 128 px. **If it doesn't fit, give way in this order:** time 4×→3× (add an `emerald3` digits font to the tool), then merge the two 1× lines, then move the menu box's values to a 2×2 grid. Never shrink the sprite or the 2× text. Ledger each.
- [ ] **Step 3: Check night.** With a demo override `const DEMO_NIGHT = true` (removed before the commit), the darker field shows. Screenshot it.
- [ ] **Step 4: Check missing data in the simulator** (weather off or no data): `--` shows in place and the HP bar is grey.
- [ ] **Step 5: Run `ciqtest.ps1`** (no regressions). Commit `feat(garmin): Pokéface v2 Emerald battle layout and font`.

### Task 4: Demo stepper and the watch

**Files:**
- Modify: `garmin/pokeface/source/PokefaceView.mc` (`DEMO` cycles weekday × {day, night, reward} by minute)

- [ ] **Step 1: Replace v1's `DEMO` table** with: index = `clock.min % 21`, so species = index, day = `index % 3 != 1`, reward = `index % 3 == 2`. Build each of the 21 by pinning the index (as v1 did). Expected: every Pokémon loads, faces right and has its name in the box. Make a contact sheet of the 21 crops in `bin/` and look at it.
- [ ] **Step 2: Set `DEMO = false`, build, and copy `Pokeface.prg`** to the watch's `GARMIN/APPS` over MTP (when the watch is plugged in).
- [ ] **Step 3: Commit** `feat(garmin): Pokéface v2 demo covers all 21`
