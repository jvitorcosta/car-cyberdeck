# Yellow deck menu, night mode and favourites: implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The yellow deck boots to a Fallout-terminal main menu. Its Lights page gains Back, an
on/link line and 4 favourites. The screen dims from 18:00 to 06:00. The Garmin app applies
favourites and shows a disconnected light.

**Architecture:** New logic goes into small **pure** modules in `src/util/`, host-tested with Unity
like the rest of the repo: the look and swatch table, favourites, light status, the screen layout and
navigation, and the terminal palette. `src/ledpanel.cpp` keeps the device side (BLE, touch, Xbox,
NVS, HTTP, drawing) and switches between two screens, Menu and Lights. Night mode reuses the black
deck's `util/daynight`, `net/clock` and `ui/backlight`. The watch app gets a fourth page and two
new deck endpoints.

**Tech Stack:** ESP32 CYD, PlatformIO env `ledpanel` (Arduino, TFT_eSPI 2.5.43, NimBLE-Arduino
2.5.1, XPT2046), Unity host tests via `tools/run_tests.ps1`, Garmin Connect IQ SDK 9.3 (Monkey C,
fr255).

**Spec:** `docs/superpowers/specs/2026-10-23-yellow-deck-menu-design.md` (parts 1 and 2 only).

## Global Constraints

- Branch `feat/yellow-menu` off `main`. Commit per task (conventional commits, `type(scope): subject`,
  scopes `leds` for the deck, `garmin` for the watch). Do not push or open a PR. The owner asks for that.
- Public repo: never commit `src/config.h`, `garmin/carlights/source/Secrets.mc` or `garmin/carlights/bin/`.
- `/say` and `/state` keep their current request and reply formats (the Siri Shortcut and the watch depend on them).
- Voice stays English. Portuguese words already in `ledvoice` stay. Add no new Portuguese.
- The left Xbox stick stays ignored (drift). D-pad only.
- `src/` builds with `-Werror` (incl. `-Wshadow`, `-Wdouble-promotion`).
- No scanlines, glow or flicker effects.
- Night: 18:00-06:00 local (`daynight`), backlight day 255 / night 90 (`ui/backlight`), the screen only, never the LEDs.
- Every touch target is at least 36 px tall and 70 px wide.
- Host tests: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1` (all suites must pass).
- Device build: `pio run -e ledpanel`. Flash: `pio run -e ledpanel -t upload` (the yellow deck on USB).
- Watch build: `monkeyc -d fr255 -f garmin/carlights/monkey.jungle -o garmin/carlights/bin/CarLights.prg -y D:\dev\tools\garmin\developer_key.der -w -l 2`.

## Review Focus

1. **Siri dictates digits**: "favourite 2" arrives as `Favourite 2.`. The 2 must pick slot 2, never set the level to 20 %. *Pinned in Task 2.*
2. **Mishearings and filler words**: "favourite for" → slot 4, "favorite to" → slot 2, but "set the lights to blue" must not touch favourites. *Pinned in Task 2.*
3. **An empty slot**: applying it changes nothing and says "Favourite 3 is empty". Its preview shows `--`, and the watch can't apply it. *Pinned in Tasks 1 and 7.*
4. **A favourite saved with a light off**: it stays off when applied, and the previews show it off. *Pinned in Task 1.*
5. **NVS from an older build, or junk**: missing or out-of-range favourite bytes load as empty slots, never as a crash or a wrong colour index. *Pinned in Task 1.*

---

## File structure

| File | Responsibility |
|---|---|
| `src/util/ledlook.h` (new) | `Swatch` table and `Look` (moved out of `ledpanel.cpp`, unchanged values) |
| `src/util/ledfavs.h/.cpp` (new) | `Fav`, NVS byte encoding, voice/HTTP description lines |
| `src/util/ledvoice.h` (modify) | `Command::fav` and favourite phrases |
| `src/util/lightstatus.h/.cpp` (new) | on/off/no-link state, the status line, the `/status` reply |
| `src/util/deckmenu.h/.cpp` (new) | Menu and Lights layouts, hit test, D-pad neighbour, hold rule |
| `src/util/termpal.h/.cpp` (new) | terminal greens, day and night |
| `src/ledpanel.cpp` (modify) | screens, drawing, favourites store, night mode, endpoints |
| `platformio.ini` (modify) | `ledpanel` filter gains `net/clock.cpp`, `ui/backlight.cpp` |
| `garmin/carlights/source/Deck.mc`, `ZonesView.mc`, `ZonesDelegate.mc` (modify), `FavDots.mc` (new) | watch favourites and link state |
| `docs/leds-voice.md`, `docs/BACKLOG.md` (modify) | how to use favourites/menu, car-test items |

---

### Task 1: Look, swatches and favourites (pure)

**Files:**
- Create: `src/util/ledlook.h`, `src/util/ledfavs.h`, `src/util/ledfavs.cpp`
- Modify: `src/ledpanel.cpp:39-71` (use `ledlook::` instead of its own `Swatch`/`Look`/`MIN_INTENSITY`)
- Test: `test/test_ledfavs/test_ledfavs.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace ledlook {
  struct Swatch { const char* name; const char* word; uint8_t r, g, b; uint32_t screen; };
  constexpr int N_SWATCH = 8;
  // same order and values as ledpanel.cpp today; word = lowercase name ("red" ... "yellow")
  inline constexpr Swatch SWATCHES[N_SWATCH] = { ... };
  constexpr uint8_t MIN_PCT = 10;
  struct Look { int swatch = N_SWATCH - 1; uint8_t pct = 100; bool lit = true; };
  inline bool operator==(const Look& a, const Look& b);
  }
  namespace ledfavs {
  constexpr int N_FAV = 4;
  constexpr int BYTES = 6;   // inSwatch, inPct, inLit, outSwatch, outPct, outLit
  struct Fav { bool set = false; ledlook::Look in, out; };
  void encode(const Fav& f, uint8_t out[BYTES]);
  // false and *f = {} (empty) unless every byte is valid: swatch < 8, pct 10..100 in steps of 10, lit 0/1
  bool decode(const uint8_t in[BYTES], Fav* f);
  // slot is 1..4. "Favourite 1: inside Pink 50 percent, outside Pink 100 percent";
  // a light that's off reads "inside off"; empty: "Favourite 3 is empty"
  void describe(const Fav& f, int slot, char* out, size_t n);
  // one /favs line: "1 pink 50 pink 100"; off light "off 0"; empty "3 -"
  void wireLine(const Fav& f, int slot, char* out, size_t n);
  }
  ```

- [ ] **Step 1: Write the failing tests** in `test/test_ledfavs/test_ledfavs.cpp`:
  - `test_encode_decode_round_trip`: Fav{set, in{2,50,true}, out{2,100,true}} → encode → decode → equal, `set` true.
  - `test_off_light_survives`: in.lit=false round-trips, and `describe` gives `"Favourite 1: inside off, outside Pink 100 percent"`.
  - `test_bad_bytes_are_empty`: `{9,50,1,0,100,1}`, `{0,55,1,0,100,1}`, `{0,50,2,0,100,1}`, `{0xFF x6}` → `decode` false and `f.set` false.
  - `test_describe_set_and_empty`: slot 1 pink 50/pink 100 → `"Favourite 1: inside Pink 50 percent, outside Pink 100 percent"`; an empty slot 3 → `"Favourite 3 is empty"`.
  - `test_wire_lines`: `"1 pink 50 pink 100"`, `"2 off 0 blue 80"`, `"3 -"`.
  - `test_swatch_words`: `SWATCHES[0].word` is `"red"`, `[7]` is `"yellow"`, and the colour order matches `ledvoice` (red, orange, pink, purple, blue, green, white, yellow).
- [ ] **Step 2: Run** `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1`. Expected: `test_ledfavs` FAILs (missing header).
- [ ] **Step 3: Implement** `ledlook.h` (move the table and `Look` from `ledpanel.cpp:44-71`, values unchanged, add `word`), then `ledfavs.h/.cpp`. Switch `ledpanel.cpp` to `ledlook::` names (`using` declarations are fine). No behaviour change.
- [ ] **Step 4: Run** the host tests (all PASS) and `pio run -e ledpanel` (builds).
- [ ] **Step 5: Commit** `refactor(leds): look and swatches in util/ledlook, favourites module`.

### Task 2: Voice favourites (pure)

**Files:**
- Modify: `src/util/ledvoice.h`
- Test: `test/test_ledvoice/test_ledvoice.cpp` (add tests)

**Interfaces:**
- Produces: `ledvoice::Command::fav`, an `int`: 1..4 for "favourite N", or -1.

Rules: a **favourite word** (`favourite`, `favorite`, `favourites`, `favorites`, `preset`) arms the
slot. The **next** number word or digit sets `fav`: `one`/`won`/`1` → 1, `two`/`to`/`too`/`2` → 2,
`three`/`tree`/`3` → 3, `four`/`for`/`fore`/`4` → 4. While armed, a digit is a slot, **not** a level,
and numbers outside 1..4 leave `fav` -1. Number words are ignored when not armed (`to`, `for` are
filler). A favourite word with no slot after it gives `fav` -1. If nothing else was parsed, `parse`
returns false.

- [ ] **Step 1: Write the failing tests:**
  - `test_favourite_digit`: `"Favourite 2."` → `fav == 2`, `level == -1`.
  - `test_favourite_words_and_mishearings`: `"favorite one"`→1, `"favourite to"`→2, `"preset three"`→3, `"favourite for"`→4, `"Favourite four please"`→4.
  - `test_filler_to_is_not_a_favourite`: `"set the lights to blue"` → `fav == -1`, `color == 4`.
  - `test_favourite_without_slot`: `parse("favourite", &c)` returns false. `"favourite 7"` returns false.
  - `test_level_still_works`: `"blue 50%"` → `level == 50`, `fav == -1`.
- [ ] **Step 2: Run** the host tests. Expected: the new tests FAIL.
- [ ] **Step 3: Implement** in `ledvoice::parse` (new `FAVS` and `SLOTS` word tables, plus a local `armed` flag).
- [ ] **Step 4: Run** the host tests. All PASS.
- [ ] **Step 5: Commit** `feat(leds): "favourite N" in voice commands`.

### Task 3: Screen layout, navigation and palette (pure)

**Files:**
- Create: `src/util/deckmenu.h`, `src/util/deckmenu.cpp`, `src/util/termpal.h`, `src/util/termpal.cpp`
- Test: `test/test_deckmenu/test_deckmenu.cpp`, `test/test_termpal/test_termpal.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace deckmenu {
  struct Rect { int16_t x, y, w, h; };
  enum class Screen : uint8_t { Menu, Lights };
  // Menu: up to 6 item slots (3 cols x 2 rows), then 4 favourites. Part 1 builds one item, Lights.
  enum MenuId : int { M_ITEM0 = 0, M_FAV0 = 6, M_N = 10 };
  enum Item : uint8_t { LIGHTS };           // later: CAR, WEATHER, HEART, MUSIC
  constexpr int N_ITEMS = 1;                // items shown, in slot order
  // Lights: Back, 3 zones, 8 tiles, 4 favourites, -, +
  enum LightsId : int { L_BACK = 0, L_ZONE0 = 1, L_TILE0 = 4, L_FAV0 = 12, L_MINUS = 16, L_PLUS = 17, L_N = 18 };
  Rect rect(Screen s, int id);              // {0,0,0,0} for an item slot not built
  constexpr Rect HEADER{0, 0, 320, 34}, MENU_STRIP{0, 184, 320, 56};          // menu, display only
  constexpr Rect STATUS{4, 40, 312, 12}, LEVEL{106, 190, 108, 46};          // lights, display only
  int hit(Screen s, int x, int y);          // nearest within 10 px, -1 if none or on a display-only rect
  int neighbour(Screen s, int from, int dx, int dy);  // as ledpanel's neighbour(): centres, sideways x2
  bool isFav(Screen s, int id);
  int favSlot(Screen s, int id);            // 0..3, or -1
  constexpr uint32_t HOLD_MS = 800;         // long-press / hold A on a Lights favourite saves it
  }
  namespace termpal {
  enum Role : uint8_t { BG, TEXT, MID, DIM };
  uint32_t rgb(Role r, bool night);         // day: BG 0x000000, TEXT 0x3CFF7A, MID 0x1F9E48, DIM 0x0F5024; night: each channel x45/100
  uint32_t tile(uint32_t screenRgb, bool night);  // day: unchanged; night: each channel x70/100
  }
  ```
- Layout values (starting points, tune on the panel). **Menu**: item slot i at `{4 + (i%3)*106, 40 + (i/3)*46, 100, 40}`, favourite k at `{4 + k*79, 134, 75, 44}`. **Lights**: Back `{4,2,70,36}`, zone z `{80 + z*80, 2, 76, 36}`, tile i `{4 + (i%4)*79, 54 + (i/4)*48, 75, 44}`, favourite k `{4 + k*79, 150, 75, 36}`, minus `{4,190,96,46}`, plus `{220,190,96,46}`. The glance (`HondaGlance`) is left as is (YAGNI).

- [ ] **Step 1: Write the failing tests:**
  - `test_rects_inside_screen_and_apart`: for both screens, every built rect lies in 320x240, no two overlap, none overlaps `LEVEL`/`STATUS`/`HEADER`/`MENU_STRIP`, and each is ≥70x36 (Global Constraints).
  - `test_unbuilt_items_have_no_rect`: `rect(Menu, M_ITEM0 + 1)` has w 0, and `hit` never returns it.
  - `test_hit_snaps_and_ignores_display_rects`: a point in a gap within 10 px of a tile hits the tile. A point inside `LEVEL` gives -1.
  - `test_neighbour`: on Lights, right from `L_TILE0` is `L_TILE0+1`, down from `L_TILE0+4` is `L_FAV0`, up from a zone is the zone itself (nothing above). On Menu, down from `M_ITEM0` reaches `M_FAV0`.
  - `test_fav_slots`: `favSlot(Lights, L_FAV0+3) == 3`, `favSlot(Menu, M_FAV0) == 0`, `favSlot(Lights, L_TILE0) == -1`.
  - `test_termpal`: day TEXT is `0x3CFF7A`. Night TEXT has every channel ≤ day's. BG is 0 in both modes. `tile(0xFF0090, false) == 0xFF0090`. `tile(0xFF0090, true) == 0xB20064`.
- [ ] **Step 2: Run** the host tests. Expected: both suites FAIL.
- [ ] **Step 3: Implement** both modules. Move the hit/neighbour logic from `ledpanel.cpp:226-240` and `555-572` into `deckmenu.cpp` as generic functions over a rect list.
- [ ] **Step 4: Run** the host tests. All PASS.
- [ ] **Step 5: Commit** `feat(leds): menu and lights layouts, terminal palette`.

### Task 4: Light status (pure)

**Files:**
- Create: `src/util/lightstatus.h`, `src/util/lightstatus.cpp`
- Test: `test/test_lightstatus/test_lightstatus.cpp`

**Interfaces:**
- Consumes: `ledlook::Look`.
- Produces:
  ```cpp
  namespace lightstatus {
  enum class State : uint8_t { NoLink, Off, On };
  State of(bool linked, bool lit);      // NoLink whenever !linked, whatever lit says
  const char* word(State s);            // "nolink", "off", "on"
  // the Lights page line: "IN ON 50%  OUT --  LINK 1/2" (OFF when off, -- when no link)
  void line(const ledlook::Look& in, bool inUp, const ledlook::Look& out, bool outUp, char* buf, size_t n);
  // GET /status reply: "inside=on outside=nolink"
  void reply(State in, State out, char* buf, size_t n);
  }
  ```
- [ ] **Step 1: Write the failing tests:**
  - `test_no_link_never_on`: `of(false, true) == NoLink`.
  - `test_line`: (pink 50 lit, up) and (pink 100 lit, up) → `"IN ON 50%  OUT ON 100%  LINK 2/2"`. (lit, up) and (lit, down) → `"IN ON 50%  OUT --  LINK 1/2"`. Off and up → `"IN OFF ..."`.
  - `test_reply`: `"inside=on outside=nolink"`.
- [ ] **Step 2: Run** the host tests. Expected: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the host tests. All PASS.
- [ ] **Step 5: Commit** `feat(leds): light status line and reply`.

### Task 5: Menu screen, Back and the terminal look (device)

**Files:**
- Modify: `src/ledpanel.cpp` (header comment, drawing, `press`, `act`, `handlePad`, `hit`/`neighbour` callers, `setup`)

**Interfaces:**
- Consumes: `deckmenu::*`, `termpal::*` (night=false until Task 7), `lightstatus::line/of`, `ledlook::*`.
- Produces: `static deckmenu::Screen screen` (current screen), `static void show(deckmenu::Screen s)` (switches screen, resets focus, full redraw), `static bool night` (false here, set in Task 7), `static void drawAll()` (redraws the current screen in the current palette).

Behaviour:
- Boot shows **Menu**. A tap or A on `LIGHTS` → `show(Lights)`. On Lights, `L_BACK` (tap or A) and **B** → `show(Menu)`. On Menu, B does nothing.
- **Menu** drawing: `HEADER` with `ROBCO INDUSTRIES (TM) TERMLINK` (TEXT) over `HONDA CITY // CARDECK OS` (MID), and a MID rule under it. Each item is `[ LIGHTS ]` with a MID border. The Xbox focus is drawn **inverted** (TEXT fill, BG text). The favourites row is left empty (drawn in Task 6). `MENU_STRIP` holds the clock (`--:--` until Task 7) and `IN`/`OUT` from `lightstatus::of`: `ON` on a box in the light's swatch colour, `OFF` in DIM, `--` in DIM.
- **Lights** drawing: the same function, laid out by `deckmenu::rect(Lights, id)`. Zones, tiles, `-`/`+` and the level box keep today's meaning. Chrome (Back, zones, `-`/`+`, borders, text) uses termpal greens instead of greys/white. The tiles stay their swatch colour. The selected-tile ring is TEXT green, the pressed look stays yellow, and the focus ring is TEXT green, no longer cyan. `STATUS` shows `lightstatus::line(...)` and redraws whenever a link comes or goes (the loop's `shown[]` block) or a look changes.
- The Xbox mapping on Lights: A presses the focus, **B = back**, X next zone page, Y on/off, LB/RB -/+. On Menu: D-pad and A only.
- Drop the zone-tab link chips (`ledpanel.cpp:267-272`): the STATUS line replaces them.
- [ ] **Step 1: Implement** as above. Remove the moved `hit`/`neighbour`/`rectOf`. Update the file's header comment to describe the two screens and the controller mapping.
- [ ] **Step 2: Run** the host tests and `pio run -e ledpanel`. Both pass.
- [ ] **Step 3: On the deck** (`pio run -e ledpanel -t upload`), check and note the results in the commit message body:
  - it boots to the green menu;
  - tapping LIGHTS opens the page, and BACK returns;
  - every colour tile, every zone, `-` and `+` hits where tapped (calibration unchanged);
  - the Xbox controller moves the inverted bar or ring, A opens and presses, and B returns;
  - switching a controller off turns its part of STATUS into `--` within ~10 s;
  - `curl "http://<deck>/say?k=<token>&text=blue"` still changes the lights while the Menu is showing, and the strip follows.
- [ ] **Step 4: Commit** `feat(leds): terminal main menu, Back and status line on the yellow deck`.

### Task 6: Favourites on the deck

**Files:**
- Modify: `src/ledpanel.cpp` (state, NVS, touch hold, Xbox hold, drawing, voice, HTTP)

**Interfaces:**
- Consumes: `ledfavs::*`, `deckmenu::isFav/favSlot/HOLD_MS`, `ledvoice::Command::fav`, `lightstatus::reply`.
- Produces: `static ledfavs::Fav favs[ledfavs::N_FAV]`, `static void applyFav(int slot0)` (slot 0..3), `static void saveFav(int slot0)`. `GET /favs?k=` → the 4 `wireLine`s joined by `\n`. `GET /status?k=` → `lightstatus::reply`. A wrong key gets 403 "Wrong key", like `/state`.

Behaviour:
- **Storage**: NVS keys `fav0`..`fav3`, `putBytes`/`getBytes` of `ledfavs::BYTES`, loaded in `load()` with `decode`. A missing or invalid key means an empty slot. Written at once on save (rare, unlike +/-).
- **Apply** (`applyFav`): if empty, do nothing (voice answers `describe`'s empty line). Otherwise `looks[INSIDE] = fav.in`, `looks[OUTSIDE] = fav.out`. `looks[BOTH]` becomes `fav.in` when both sides are equal, else it keeps its swatch and level, with `lit = in.lit || out.lit`. *Builder's call, recorded here*: the BOTH page then shows the STATUS line as the truth. Then push both lights, `save()`, and redraw the current screen.
- **Save** (`saveFav`): `{set=true, in=looks[INSIDE], out=looks[OUTSIDE]}`, write the key, redraw that slot with a 300 ms TEXT-green flash.
- **Touch on a favourite**: these act on **release** (not on settle like the other buttons). Released before `HOLD_MS` → apply. Held to `HOLD_MS` on the Lights page → save, fired once while still held, and the release then does nothing. On the Menu, holding just applies on release.
- **Xbox A on a focused favourite**: the same as touch. Press, then release before 1 s → apply. Held 1 s on Lights → save.
- **Drawing**: each slot shows `F1`..`F4`, then a small square per light in its swatch colour (DIM outline when off), with the levels as text when they fit. Empty slots show `F3 --` in DIM. The same drawing on the Menu row and the Lights row.
- **Voice**: in `applyVoice`, a `c.fav > 0` job applies slot `fav-1` (whatever zone was asked), and the reply is `ledfavs::describe`. The reply buffer is 64 bytes today. Raise `voiceReply` and the line buffers to 112 so the longest favourite line fits.
- [ ] **Step 1: Implement** as above. Register `/favs` and `/status` in `beginVoice()`.
- [ ] **Step 2: Run** the host tests and `pio run -e ledpanel`. Both pass.
- [ ] **Step 3: On the deck**, check:
  - holding F1 on Lights saves (it flashes) and F1 shows the two looks;
  - a tap on F1 from the Menu applies it;
  - a restart keeps F1;
  - an empty F3 does nothing;
  - `curl ".../say?k=<token>&text=favourite%201"` applies it and answers with the describe line, and `text=favourite%203` answers "Favourite 3 is empty";
  - `curl ".../favs?k=<token>"` gives 4 lines, and `.../status?k=<token>` gives `inside=… outside=…`;
  - `/state` and `/say text=purple` replies are byte-for-byte as before.
- [ ] **Step 4: Commit** `feat(leds): four favourites on the yellow deck (touch, Xbox, voice, /favs)`.

### Task 7: Night mode

**Files:**
- Modify: `platformio.ini` (`[env:ledpanel]` `build_src_filter` adds `+<net/clock.cpp> +<ui/backlight.cpp>`), `src/ledpanel.cpp`

**Interfaces:**
- Consumes: `netclock::begin()`, `netclock::minuteOfDay(int*)`, `daynight::isNight(int)`, `backlight::begin()`, `backlight::set(bool night)`, `termpal::*`, Task 5's `night` and `drawAll()`.

Behaviour:
- `setup()`: replace the `PIN_BL` HIGH with `backlight::begin()`. The `voiceTask` calls `netclock::begin()` once online, next to `server->begin()`. (Without `REMOTE_TOKEN` there is no WiFi, so the deck stays in day mode. That's accepted.)
- `loop()`: every 60 s (and on the first loop after the clock syncs), `want = minuteOfDay(&m) && isNight(m)`. On a change: set `night`, call `backlight::set(night)` **before** `drawAll()`, so the repaint never shows at day brightness. All colours come from `termpal::rgb(role, night)`, tiles and light previews through `termpal::tile`, and the pressed yellow is scaled the same way.
- Menu strip: the clock `HH:MM` from `minuteOfDay`, refreshed each minute.
- [ ] **Step 1: Implement.**
- [ ] **Step 2: Run** the host tests and `pio run -e ledpanel`. Both pass.
- [ ] **Step 3: On the deck**, to test without waiting for 18:00, temporarily build with `-D NIGHT_TEST` that forces `want = true`, and remove it after. Check that the backlight drops and the greens and tiles dim with no bright frame in between, and the tiles are still told apart. Then a real build at 18:00 (or with `TIMEZONE` shifted) switches by itself and shows the right clock.
- [ ] **Step 4: Commit** `feat(leds): night mode on the yellow deck (18:00-06:00, like the black deck)`.

### Task 8: Garmin favourites and link state

**Files:**
- Create: `garmin/carlights/source/FavDots.mc`
- Modify: `garmin/carlights/source/Deck.mc`, `ZonesView.mc`, `ZonesDelegate.mc`, `CarLightsApp.mc` (header comment)

**Interfaces:**
- Consumes: `GET /favs` lines (`"1 pink 50 pink 100"`, `"2 off 0 blue 80"`, `"3 -"`), `GET /status` (`"inside=on outside=nolink"`), `/say?text=favourite N`.
- Produces (Deck.mc): `const FAVS = 3;` (page and job marker), `var favs as Array` (4 entries, each `null` or `[inColor, inPct, inLit, outColor, outPct, outLit]`, colours as `COLORS` indices found by the word in `COLORS[i][1]`), `var links as Array<String>` (`["on"|"off"|"nolink"|"", ...]` for inside, outside), `function applyFav(n as Number) as Void` (1..4).

Behaviour:
- `sync()` also queues `/favs` and `/status` (4 jobs, under the queue cap of 6). `onReply` branches on the job's path: `/favs` fills `favs`, `/status` fills `links`, and `/say` with the FAVS marker does **not** `absorb` its reply ("Favourite 1: …" would misread), but sets `status` to "" and calls `sync()`.
- `FavDots` (a Drawable): two `ColorDot`-style circles side by side, the inside and outside colours, dark grey when that light is off.
- Pages: UP/DOWN loop over 4 pages, BOTH → INSIDE → OUTSIDE → FAVOURITES. The page dots become 4. The Favourites page draws `FAVOURITES` in a pill on top, then 4 rows: `F1` plus two dots, or `F3 --` for empty, then the `pill` "START: apply" (or the status).
- START on the Favourites page opens a `Menu2` titled "Favourites" with an `IconMenuItem` per **set** slot (label `F1`, sublabel such as "Pink 50 / Pink 100", icon `FavDots`). Selecting one calls `applyFav` and pops back. With no set slots, the menu holds one item, "None saved", which does nothing.
- Link state: on INSIDE/OUTSIDE, when `links` says `nolink`, the bottom text (not busy) becomes "Not connected". On BOTH, the bottom pill shows "Inside not connected" / "Outside not connected" / "Not connected" when there's no status message. `status` (sending, errors) still wins.
- [ ] **Step 1: Implement.**
- [ ] **Step 2: Build** with the watch build command (Global Constraints). Expected: BUILD SUCCESSFUL with no warnings about new code.
- [ ] **Step 3: On the watch** (sideload to `GARMIN/APPS` over MTP as before, with the deck on the hotspot), check:
  - the 4th page lists the deck's favourites;
  - START → F1 applies it, and the BOTH page then shows the new looks;
  - switching one controller off shows "Not connected" after reopening the app.
- [ ] **Step 4: Commit** `feat(garmin): favourites page and not-connected status`.

### Task 9: Docs

**Files:**
- Modify: `docs/leds-voice.md` (menu, Back, favourites by touch/Xbox/voice/watch, night mode, `/favs` and `/status`), `docs/BACKLOG.md` (Verification: the car test of the menu, favourites and night mode, and the Xbox B change; Ideas: parts 3-7 of the spec pointing at it)
- [ ] **Step 1: Write** both updates.
- [ ] **Step 2: Commit** `docs(leds): menu, favourites and night mode`.
