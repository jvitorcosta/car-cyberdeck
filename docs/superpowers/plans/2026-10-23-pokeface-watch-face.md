# Pokéface Watch Face Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A Connect IQ watch face for the Forerunner 255 with a Charmander-line partner that changes over the day, Mega-evolves when earned, and sits among all of João's chosen data.

**Architecture:** A new Connect IQ project, `garmin/pokeface/`, next to `garmin/carlights/`. Pure logic (which Pokémon, the Mega rule, the flame size, text formatting) lives in two modules with no device calls, covered by Monkey C `(:test)` unit tests. A Python tool fetches the Black/White animated GIFs into a git-ignored folder and turns them into half-size 64-colour PNG frames plus a generated `sprites.xml`, also git-ignored. The view reads the device data, asks the logic modules what to draw, and draws it.

**Tech Stack:** Monkey C, Connect IQ SDK 9.3.0 (fr255 is CIQ 5.2: 260×260 MIP, 8 bpp/64 colours, 128 KB watch-face memory), Python 3 + Pillow for the sprite tool, `unittest`.

**Spec:** `docs/superpowers/specs/2026-10-23-pokeface-watch-face-design.md`

## Global Constraints

- Products: `fr255` and `fr255m` only. App type `watchface`, `minApiLevel="3.2.0"`, app id `f74f9786dfa8488d85883e4c563291ab`.
- Watch-face memory limit is 131072 bytes. Only the current Pokémon's frames are loaded at any time.
- No Nintendo/Smogon art in git (the repo is **public**). Git-ignore `pokeface-sprites/`, `garmin/pokeface/resources/sprites/` and `garmin/pokeface/bin/`.
- Downloads happen only after João approves these exact URLs (Task 2, step 1):
  `https://play.pokemonshowdown.com/sprites/gen5ani/{charmander,charmeleon,charizard,charizard-megax,charizard-megay}.gif`
- Sprites are scaled **down 2× with nearest-neighbour sampling** (96→48 class, blocky, never blurred) and snapped to the 64-colour palette (each channel one of 0x00, 0x55, 0xAA, 0xFF).
- Hours: Charmander 06–11, Charmeleon 12–17, Charizard 18–21, asleep 22–05. Mega (X or Y, random, rolled once per calendar day) whenever earned; earned = a run finished today **or** steps ≥ step goal (goal > 0).
- Asleep: Charizard, or the day's Mega if earned; one darkened frame, "Zzz", dim flame; no idle animation at night.
- Temperature is from `Weather.getCurrentConditions()`, never the wrist sensor. Respect `DeviceSettings` for 12/24 h, temperature units and distance units.
- Any value that isn't available shows `--`, and the face never crashes.
- Commits use conventional commits, `type(garmin): subject`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

**Build/test commands** (PowerShell, from `garmin/pokeface/`; `$sdk` is the path in `%APPDATA%\Garmin\ConnectIQ\current-sdk.cfg`; the developer key is `D:\dev\tools\garmin\developer_key.der`, outside the repo):
- Build: `& "$sdk\bin\monkeyc.bat" -d fr255 -f monkey.jungle -o bin\Pokeface.prg -y D:\dev\tools\garmin\developer_key.der -l 2`
- Unit tests: start the simulator once (`& "$sdk\bin\connectiq.bat"`), then build with `--unit-test -o bin\PokefaceTest.prg` added, and run `& "$sdk\bin\monkeydo.bat" bin\PokefaceTest.prg fr255 -t`. Pass = the summary line reports every test PASSED and none FAILED.
- Sprite tool tests: `python tools/test_gen_pokeface.py` (CI already runs `python -m unittest discover -s tools -p "test_*.py"`).

## Review Focus

1. **Midnight with yesterday's Mega stored:** at 00:00 the stored `[dayKey, choice]` belongs to yesterday, so the partner must be Charizard asleep, not yesterday's Mega. → `testMegaResetsOnNewDay` (Task 1).
2. **No phone/weather/HR/Body Battery/sun data** (watch just rebooted, phone off): every field shows `--`. → `testFmtMissingValues` (Task 1) and the simulator check with weather disabled (Task 3).
3. **Step goal of 0 or missing:** must not count as earned, and the step bar must not divide by zero. → `testZeroGoalNotEarned`, `testBarFractionZeroGoal` (Task 1).
4. **A GIF with fewer frames than wanted** (a Mega GIF may be short): frames repeat in a cycle, so the count stays fixed and `sprites.xml` stays valid. → `test_short_gif_cycles` (Task 2).
5. **12-hour watches and imperial units:** "1:05" not "13:05", °F, miles. → `testFmtClock12h`, `testFmtImperial` (Task 1).

---

### Task 1: Project scaffold and the pure logic, with unit tests

**Files:**
- Create: `garmin/pokeface/manifest.xml`, `garmin/pokeface/monkey.jungle`, `garmin/pokeface/resources/strings/strings.xml`, `garmin/pokeface/resources/drawables/drawables.xml` (launcher icon only; copy carlights' `launcher_icon.png` for now)
- Create: `garmin/pokeface/source/PokefaceApp.mc` (returns an empty `PokefaceView` for now)
- Create: `garmin/pokeface/source/Partner.mc`, `garmin/pokeface/source/Fmt.mc`
- Create: `garmin/pokeface/source/Tests.mc`
- Modify: `.gitignore` (add the three ignores from Global Constraints under the existing "Garmin watch app" block)

**Interfaces:**
- Produces, `module Partner`:
  - `enum { CHARMANDER, CHARMELEON, CHARIZARD, MEGA_X, MEGA_Y }`
  - `function asleep(hour as Number) as Boolean`: true for 22–23 and 0–5
  - `function pick(hour as Number, earned as Boolean, mega as Number) as Number`: awake → by hour, or `mega` (MEGA_X/MEGA_Y) if earned; asleep → `mega` if earned, else CHARIZARD
  - `function earned(steps as Number or Null, goal as Number or Null, ranToday as Boolean) as Boolean`
  - `function dayKey(y as Number, m as Number, d as Number) as Number`: `y * 10000 + m * 100 + d`
  - `function megaFor(today as Number, stored as Array or Null, roll as Number) as Array`: returns `[today, choice]`; keeps `stored` if `stored[0] == today`, otherwise `[today, roll == 0 ? MEGA_X : MEGA_Y]`
  - `function flameRadius(bb as Number or Null, asleep as Boolean) as Number`: asleep → 2; null → 5; else `2 + clamp(bb,0,100) * 6 / 100`
  - `function barFraction(steps as Number or Null, goal as Number or Null) as Float`: 0.0 when either is null or goal ≤ 0, capped at 1.0
- Produces, `module Fmt`:
  - `function num(v as Number or Null) as String`: `"--"` for null
  - `function clock(hour as Number, min as Number, is24 as Boolean) as String`: `"13:05"` / `"1:05"` (12 h: 0 → 12, no leading zero on the hour)
  - `function temp(c as Number or Float or Null, metric as Boolean) as String`: `"24°"` / `"75°"`, `"--"` for null
  - `function dist(cm as Number or Null, metric as Boolean) as String`: one decimal, `"4.8 km"` / `"3.0 mi"`, `"--"` for null
  - `function nextSun(now as Number, rise as Number or Null, set as Number or Null, is24 as Boolean) as String`: epoch seconds in; whichever of rise/set is next and after `now`, as `"↑06:12"` / `"↓18:02"` in local time via `clock`; `"--"` if neither is known

- [ ] **Step 1: Write the scaffold** with the manifest values from Global Constraints (permissions `SensorHistory`, `UserProfile`; language `eng`), `monkey.jungle` = `project.manifest = manifest.xml`, and the `.gitignore` lines. Build with the Build command. Expected: `BUILD SUCCESSFUL`.
- [ ] **Step 2: Write the failing tests** in `Tests.mc`, each a `(:test) function name(logger as Test.Logger) as Boolean` using `Test.assertEqual`:
  - `testStages`: `pick(6,false,MEGA_X)==CHARMANDER`, `pick(11,…)==CHARMANDER`, `pick(12,…)==CHARMELEON`, `pick(17,…)==CHARMELEON`, `pick(18,…)==CHARIZARD`, `pick(21,…)==CHARIZARD`
  - `testAsleep`: `asleep(22)`, `asleep(5)`, `!asleep(6)`, `!asleep(21)`; `pick(23,false,MEGA_Y)==CHARIZARD`; `pick(23,true,MEGA_Y)==MEGA_Y`; `pick(3,false,MEGA_X)==CHARIZARD`
  - `testEarnedOverridesDay`: `pick(9,true,MEGA_X)==MEGA_X`
  - `testEarned`: `earned(10000,10000,false)`, `!earned(9999,10000,false)`, `earned(0,10000,true)`, `!earned(null,null,false)`
  - `testZeroGoalNotEarned`: `!earned(500,0,false)`
  - `testMegaResetsOnNewDay`: `megaFor(20261024,[20261023,MEGA_X],1) == [20261024,MEGA_Y]`; `megaFor(20261023,[20261023,MEGA_X],1) == [20261023,MEGA_X]`; `megaFor(20261023,null,0) == [20261023,MEGA_X]`
  - `testFlame`: `flameRadius(100,false)==8`, `flameRadius(0,false)==2`, `flameRadius(150,false)==8`, `flameRadius(null,false)==5`, `flameRadius(100,true)==2`
  - `testBarFractionZeroGoal`: `barFraction(500,0)==0.0`, `barFraction(null,10000)==0.0`, `barFraction(15000,10000)==1.0`
  - `testFmtMissingValues`: `num(null)`, `temp(null,true)`, `dist(null,true)`, `nextSun(0,null,null,true)` all `"--"`
  - `testFmtClock12h`: `clock(13,5,false)=="1:05"`, `clock(0,7,false)=="12:07"`, `clock(13,5,true)=="13:05"`
  - `testFmtImperial`: `temp(24,false)=="75°"`, `dist(482803,false)=="3.0 mi"`, `dist(480000,true)=="4.8 km"`
  - `testNextSun`: with `rise=1000, set=5000, is24=true`: `now=500` → starts `"↑"`; `now=2000` → starts `"↓"`; `now=6000` → `"--"`
- [ ] **Step 3: Run the unit tests.** Expected: they fail to compile or FAIL (Partner/Fmt are empty).
- [ ] **Step 4: Implement `Partner` and `Fmt`** to the Interfaces above. `Fmt.nextSun` formats with `Gregorian.info(new Time.Moment(t), Time.FORMAT_SHORT)`.
- [ ] **Step 5: Run the unit tests.** Expected: all PASSED.
- [ ] **Step 6: Commit** `feat(garmin): Pokéface scaffold with partner and format logic`

### Task 2: Sprite tool: fetch, cut, halve, snap

**Files:**
- Create: `tools/gen_pokeface.py`, `tools/test_gen_pokeface.py`

**Interfaces:**
- Produces, on disk (all git-ignored): `pokeface-sprites/<name>.gif`; `garmin/pokeface/resources/sprites/<name>_<i>.png` for `i` in `0..FRAMES-1`; `<name>_sleep.png` for `charizard`, `charizard-megax`, `charizard-megay`; `garmin/pokeface/resources/sprites/sprites.xml` with one `<bitmap id="<Id><i>" filename="<name>_<i>.png" dithering="none"/>` per frame (Ids `Charmander`, `Charmeleon`, `Charizard`, `MegaX`, `MegaY`, sleep ids `CharizardSleep`, `MegaXSleep`, `MegaYSleep`).
- Produces, Python: `FRAMES = 6`, `PALETTE_LEVELS = (0x00, 0x55, 0xAA, 0xFF)`, and
  - `pick_frames(frames: list[Image], n: int) -> list[Image]`: n frames evenly spaced over the GIF; cycles when the GIF has fewer than n
  - `crop_union(frames) -> list[Image]`: all frames cropped to the union bounding box of their opaque pixels
  - `halve(img) -> Image`: `img.resize((w//2, h//2), Image.NEAREST)` on RGBA
  - `snap(img) -> Image`: each RGB channel to the nearest `PALETTE_LEVELS`; alpha < 128 → fully transparent, else opaque
  - `darken(img) -> Image`: RGB × 0.5, then `snap`
  - `main()`: fetch the 5 URLs if missing (`urllib.request`, like `gen_shadow_rider.py`), then the pipeline, then `sprites.xml`; prints each Pokémon's frame size
- Task 4 consumes the bitmap ids and `FRAMES = 6`.

- [ ] **Step 1: Ask João to approve the five download URLs** listed in Global Constraints (source: Pokémon Showdown's `gen5ani` set, Black/White sprites plus the Smogon sprite project's Megas). Do not run `main()` until he says yes.
- [ ] **Step 2: Write the failing tests** (`unittest`, synthetic Pillow images, no network; module docstring in the style of `test_gen_shadow_rider.py`):
  - `test_pick_frames_spread`: 12 frames, n=6 → indices 0,2,4,6,8,10
  - `test_short_gif_cycles`: 4 frames, n=6 → indices 0,1,2,3,0,1
  - `test_crop_union`: two 96×96 frames with opaque pixels at (10,20) and (50,70) → both 41×51
  - `test_halve_is_nearest`: a 4×4 checker of two colours halves to 2×2 using only those two colours (no blended colour)
  - `test_snap`: (0x60,0x10,0xF0,255) → (0x55,0x00,0xFF,255); (1,2,3,100) → alpha 0
  - `test_darken`: (0xFF,0xAA,0x55,255) → (0xAA,0x55,0x00,255)
- [ ] **Step 3: Run** `python tools/test_gen_pokeface.py`. Expected: FAIL (module missing).
- [ ] **Step 4: Implement `gen_pokeface.py`** (module docstring: what it builds, the source, that the art is Nintendo's/Smogon's and never committed, how to run it).
- [ ] **Step 5: Run the tests.** Expected: OK.
- [ ] **Step 6: Run `python tools/gen_pokeface.py`** (after approval). Expected: 5 GIFs in `pokeface-sprites/`, 33 PNGs + `sprites.xml`, and printed sizes around 48×48 or smaller. Check `git status` shows none of them.
- [ ] **Step 7: Commit** `feat(garmin): sprite tool for the Pokéface partner`

### Task 3: The face layout with live data ("A, all at once")

**Files:**
- Create: `garmin/pokeface/source/PokefaceView.mc`, `garmin/pokeface/source/Stats.mc`
- Modify: `garmin/pokeface/source/PokefaceApp.mc`

**Interfaces:**
- Consumes: `Fmt.*`, `Partner.barFraction` (Task 1).
- Produces, `module Stats` (device reads; every function returns null when the data isn't there and catches nothing it doesn't need to):
  - `heartRate() as Number or Null`: `Activity.getActivityInfo().currentHeartRate`, else the newest `ActivityMonitor.getHeartRateHistory(1, true)` sample
  - `bodyBattery() as Number or Null`: newest `SensorHistory.getBodyBatteryHistory({:period => 1})` sample
  - `weather() as Weather.CurrentConditions or Null`
  - `today() as ActivityMonitor.Info`: steps, stepGoal, calories, distance
  - `sun(cond) as [Number or Null, Number or Null]`: `Weather.getSunrise/getSunset(cond.observationLocationPosition, Time.today())`, epoch seconds, nulls without a position
- Produces, `PokefaceView.drawPartnerAt(dc, cx, cy)`: a placeholder for Task 4 (draws an orange 48×48 rectangle).

- [ ] **Step 1: Implement `Stats`** and the view's `onUpdate`, drawing on black with `FONT_XTINY` for small rows and `FONT_NUMBER_MEDIUM` for the time. Starting positions (y = text centre; tune to fit, within this order):
  - y 22: weather condition glyph (a simple drawn icon or a one-character code per `Weather.CONDITION_*` group: sun, cloud, rain, storm, snow, fog), `high° low°`, `Fmt.nextSun`
  - y 44: `WED 23` (`Gregorian.info(Time.now(), Time.FORMAT_MEDIUM)` day-of-week uppercase + day), a filled dot when `System.getDeviceSettings().phoneConnected` (hollow when not), `✉<notificationCount>`, `<battery>%`
  - y 88: `Fmt.clock`
  - Sprite centre (130, 150). Left of it, right-justified at x 96: `♥ <hr>` (y 140, red) and the temperature (y 160). Right of it, left-justified at x 164: `BB <bb>` (y 140, cyan) and `<kcal> kcal` (y 160)
  - y 198: step bar, x 75–185, 5 px, orange fill = `Partner.barFraction`
  - y 216: `<steps> · Fmt.dist(...)`
- [ ] **Step 2: Build and run in the simulator** (`monkeydo bin\Pokeface.prg fr255`). Expected: every row visible inside the round screen, nothing clipped, all text readable. Take a screenshot (Simulator → File → Save Screenshot) to `garmin/pokeface/bin/layout.png` and look at it.
- [ ] **Step 3: Simulate missing data** (Simulator → Settings: disconnect the phone, clear weather; and no activity data). Expected: `--` in every affected field, no crash in the simulator console.
- [ ] **Step 4: Switch the simulator to 12 h and statute units.** Expected: "1:05"-style time, °F, mi.
- [ ] **Step 5: Commit** `feat(garmin): Pokéface layout with weather, health and phone data`

### Task 4: The partner: frames, Mega, sleep, flame, wrist-raise animation

**Files:**
- Create: `garmin/pokeface/source/Sprite.mc`
- Modify: `garmin/pokeface/source/PokefaceView.mc` (replace the placeholder), `garmin/pokeface/monkey.jungle` (add `base.resourcePath = resources;resources/sprites`)

**Interfaces:**
- Consumes: `Partner.pick/asleep/earned/dayKey/megaFor/flameRadius` (Task 1); the bitmap ids and `FRAMES = 6` (Task 2); `Stats.today/bodyBattery` (Task 3).
- Produces, `class Sprite`:
  - `function show(species as Number, sleeping as Boolean) as Void`: when either changes, drops the old bitmaps and `WatchUi.loadResource`s only this species' frames (6, or the 1 sleep frame)
  - `function draw(dc, cx, cy, frame as Number, flame as Number) as Void`: draws the frame centred, then a flame (two filled circles, radius `flame` outside and `flame / 2` inside) at that species' tail point. Colours: orange 0xFF5500 / yellow 0xFFFF00, except Mega X's blue 0x0055FF / cyan 0x55FFFF. When asleep both are darker (0xAA5500 / 0xAAAA00; Mega X 0x0000AA / 0x0055AA)
  - `const TAIL = [[dx, dy] ×5]`: the tail tip of each species relative to the frame centre, **measured by eye from frame 0** of the generated PNGs (the sleep frame uses the same point)
- Ran today: `UserProfile.getUserActivityHistory()` iterated for `type == Activity.SPORT_RUNNING` with a `startTime` on or after `Time.today()`.
- The day's Mega: `Application.Storage` key `"mega"` holds `Partner.megaFor(...)`'s array; roll = `Math.rand() % 2` (seed `Math.srand` once from `Time.now().value()`).

- [ ] **Step 1: Implement `Sprite`** and wire it into `onUpdate`: `pick` the species from the hour, earned and the stored Mega; call `show`; during awake hours in low-power mode, frame = `minute % FRAMES`; `flameRadius(Stats.bodyBattery(), asleep)`. Asleep: draw the sleep frame and a small `z Z` at upper-right of the sprite in `COLOR_LT_GRAY`, and no animation.
- [ ] **Step 2: Wrist-raise animation:** in `onExitSleep` (awake hours only) start a `Timer.Timer` at 125 ms that advances the frame and calls `WatchUi.requestUpdate()`; stop it in `onEnterSleep`. If the simulator says timers aren't allowed for watch faces, fall back to advancing one frame per `onUpdate` while in high power (about 1 per second) and note that in the commit message.
- [ ] **Step 3: Add a simulator demo**, like the deck's `-D OTA_DEMO`: an `(:debug)` annotated function that, when a `DEMO` const is true, overrides the hour and earned flag to step through 06, 12, 18, earned (X and Y), 23 asleep, 03 asleep once per minute.
- [ ] **Step 4: Run the demo in the simulator.** Expected: each stage shows its own Pokémon, Megas appear, sleep is darkened with "z Z", flame size follows Body Battery (simulate 100 and 5). Check the simulator's memory view: peak stays under 131072 bytes on every stage.
- [ ] **Step 5: Turn the demo off, rebuild, and commit** `feat(garmin): Pokéface partner evolves over the day and goes Mega when earned`

### Task 5: On the watch

**Files:**
- Modify: `garmin/pokeface/manifest.xml` only if the watch rejects it

- [ ] **Step 1: Build the release `.prg` and copy it** to the 255's `Internal Storage/GARMIN/APPS` over MTP (the same Shell.Application `CopyHere` used for carlights), then unplug. João picks it under Watch Face on the watch.
- [ ] **Step 2: João checks on his wrist:** readable at a glance, a wrist raise animates, Mega appears after a run or the step goal, and next morning it's Charmander again.
- [ ] **Step 3: João compares battery** over a day or two against his stock face. A clearly faster drain is a failure (spec anti-goal); if so, cut the timer to 250 ms or drop to the 1 Hz fallback.
