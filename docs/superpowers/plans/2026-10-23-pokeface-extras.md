# Pokéface Extras Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the following, and clear the four deferred minors:
- shiny rewards on run days;
- the "A wild … appeared!" moment;
- the run card;
- Lv as weekly km toward 50 km;
- status badges;
- the weekly Pokédex;
- awake-only weather.

**Architecture:**
- New pure logic goes in a `Runs` module (activity summary, pace, level, EXP fraction) and in `Partner` (week start, badge, Pokédex count, "appeared" state, freshness). All of it is unit-tested.
- `Stats` gains two readers: the activity history as plain arrays, and `ActivityMonitor.getHistory()` step-goal days.
- The view reads run data on show/wake and at midnight, and reads everything else once a minute.
- The tool adds shiny sheets for the 7 rewards.
- `Battle` gains badge and weather drawing.

**Tech Stack:** Monkey C (CIQ 5.2, fr255), Python 3 + Pillow 12.1+, `unittest`.

**Spec:** `docs/superpowers/specs/2026-10-23-pokeface-extras-design.md` (on top of `2026-10-23-pokeface-emerald-design.md`, including its "As built" section)

## Global Constraints

- **Branch:** `feat/pokeface`. **Memory:** stay under 131072 bytes; the simulator currently shows about 20 KB.
- **New downloads:** 7 files, `graphics/pokemon/<folder>/shiny.pal` for latias, suicune, latios, jirachi, dialga, necrozma and zygarde, from the same pinned `BASE` (e8914bd…). João approves them before the first fetch. They are git-ignored like the rest.
- **Shiny:** the reward species (index % 3 == 2) is drawn shiny **iff a run finished today**. Otherwise it is drawn normally.
- **"Appeared" line:**
  - The state is stored per day as `[dayKey, s]`: s = 0 nothing shown, 1 "A wild" shown, 2 "A shiny" shown.
  - It is decided on show/wake only.
  - Its text is `A wild <NAME> appeared!` / `A shiny <NAME> appeared!`, shown in the dialogue box for that wake (while the timer runs). After that the stats come back.
- **Run card:** the dialogue row 2 shows `<dist> <pace>` (e.g. `8.2 km 5:12/km`; `/mi` and miles with statute units) for today's latest run, or today's distance when there was no run today.
- **Level:** `Lv` = floor(this week's running km), with the week starting Monday 00:00 local. The EXP bar = weekly km / `WEEK_GOAL_KM` = 50, capped at 1. The level is always in km. The info box's step line stays `N steps`.
- **Badge** (one, priority order): PSN if battery ≤ 15, else PAR if the phone is disconnected, else SLP if night and Body Battery ≤ 25, else none. Colours: PSN 0xAA00AA, PAR 0xAAAA00, SLP 0x555555, white text, as a 2-px-radius tag right of `Lv`.
- **DEX:** `DEX n/7` at the left of the BB row. n counts the distinct days in [Monday, today] with a run finished that day or steps ≥ that day's goal (goal > 0). It is computed from history and not stored.
- **Weather:** drawn only while the animation timer runs, over the field and under the boxes and Pokémon:
  - RAIN: diagonal streaks for the rain and storm groups;
  - SNOW: falling dots for snow;
  - SAND: horizontal tan dashes for the new SAND group. `Icons.group` moves `CONDITION_DUST`, `CONDITION_SAND`, `CONDITION_SANDSTORM` and `CONDITION_HAZE` from FOG to a new `SAND`.
- **Minors:**
  - **Sky and Pokémon:** day/night is computed once in `refresh` (`_night`) and used for both.
  - **Heart rate:** a history sample older than 900 s shows as `--`.
  - **Run data:** read only on show/wake and at the first update of a new day.
  - **Sprite tool:** `colourise` raises `ValueError` for a non-`P` source or an index beyond the palette.
- **Run data source:**
  - `UserProfile.getUserActivityHistory()` entries become `[type, startSecs, durationSecs, distanceMeters]`.
  - Confirm the distance unit in the simulator in Task 3 and ledger it.
  - Keep v1's FIT-epoch rule: a start more than 10 years old gets +631065600.
- **Commands:** reuse `.superpowers/sdd/2026-10-23-pokeface-emerald/{ciqtest,build,shot}.ps1` (copy them into this plan's workspace). Screenshots use the PrintWindow `shot.ps1` only.
- **Commits:** `type(garmin): subject` plus the Co-Authored-By line.

## Review Focus

1. **A second run after the first, same day:** on the next wake the run card shows the later run and the Lv includes both. Test: `testSummaryLatestRun` (Task 2), and a demo check in Task 5.
2. **Step goal first, then a run:** the line goes "A wild" then "A shiny", and never shows twice. Test: `testAppearUpgrade` (Task 2).
3. **Monday morning:** the week starts fresh, so Lv0 and DEX counts only today. A Sunday run must not count. Test: `testWeekStartMonday`, `testDexIgnoresLastWeek` (Task 2).
4. **No activity history or getHistory unsupported:** Lv0, an empty EXP bar, DEX 0/7 (or today only), no crash. Test: `testSummaryEmpty` (Task 2), plus `has` guards in Task 3.
5. **Zero-duration or zero-distance run** (treadmill without distance): the pace shows `--`, never a divide by zero. Test: `testPaceZero` (Task 2).

---

### Task 1: Shiny sheets and a stricter colourise

**Files:** Modify: `tools/gen_pokeface.py`, `tools/test_gen_pokeface.py`

**Interfaces:**
- Produces: `REWARDS = [2, 5, 8, 11, 14, 17, 20]`. For each reward, the file `mon<i>s.png` (128×64, shiny palette), and in `gen.xml` the bitmap `Shiny<i>`, so 7 more `<bitmap ` entries (30 in all).
- `colourise(frame, pal)` raises `ValueError("... not palette-indexed")` when `frame.mode != "P"`, and `ValueError("... index N beyond palette")` when an index ≥ `len(pal)`.

- [ ] **Step 1: Ask João to approve the 7 `shiny.pal` URLs.**
- [ ] **Step 2: Write the failing tests:**
  - `test_colourise_rejects_rgba`: an RGBA image raises ValueError.
  - `test_colourise_rejects_index_beyond_palette`: a P image with index 5 and a 3-entry palette raises ValueError.
  - `test_gen_xml_has_shiny_rewards`: 30 `<bitmap ` entries, plus `id="Shiny2"` and `filename="mon20s.png"`.
- [ ] **Step 3: Run** `python tools/test_gen_pokeface.py`. Expected: FAIL.
- [ ] **Step 4: Implement it, then run the tests.** Expected: OK.
- [ ] **Step 5: Run `python tools/gen_pokeface.py`.** Expected: `mon2s.png`…`mon20s.png` exist and their colours differ from the normal sheets (print and look at one pair).
- [ ] **Step 6: Commit** `feat(garmin): Pokéface tool builds shiny reward sheets`

### Task 2: Pure logic for runs, week, badge, Pokédex, "appeared"

**Files:** Create: `garmin/pokeface/source/Runs.mc`. Modify: `garmin/pokeface/source/Partner.mc`, `garmin/pokeface/source/Icons.mc` (SAND group), `garmin/pokeface/source/Tests.mc`

**Interfaces (produces):**
- `module Runs`:
  - `const WEEK_GOAL_KM = 50`
  - `function startSecs(start as Number, now as Number) as Number`: the FIT-epoch fix. `Partner.finishedToday` calls it.
  - `function summary(acts as Array<Array>, now as Number, todayStart as Number, weekStart as Number) as Array`: returns `[ranToday as Boolean, lastMeters as Number or Null, lastSecs as Number or Null, weekMeters as Number]`. Each act is `[type, start, duration, meters]` and only `type == Activity.SPORT_RUNNING` counts. "Today" means it finished today; "latest" means the greatest finish today; the week sums runs finishing in [weekStart, now].
  - `function level(weekMeters as Number) as Number`: whole km.
  - `function expFraction(weekMeters as Number) as Float`: capped at 1.0.
  - `function pace(meters as Number or Null, secs as Number or Null, metric as Boolean) as String`: `"5:12/km"` / `"8:22/mi"`, `"--"` for null or 0.
  - `function card(meters as Number or Null, secs as Number or Null, metric as Boolean) as String`: `Fmt.dist(meters * 100, metric) + " " + pace(...)`.
- `Partner`:
  - `function weekStart(todayStart as Number, dayOfWeek as Number) as Number`: `todayStart - row(dayOfWeek) * 86400`.
  - `enum { BADGE_NONE, BADGE_PSN, BADGE_PAR, BADGE_SLP }` and `function badge(battery as Number, phone as Boolean, night as Boolean, bb as Number or Null) as Number`.
  - `function dexCount(days as Array<Number>, weekStart as Number, todayStart as Number) as Number`: counts distinct day-start values in [weekStart, todayStart].
  - `enum { APPEAR_NONE, APPEAR_WILD, APPEAR_SHINY }` and `function appear(shown as Number, earned as Boolean, shiny as Boolean) as Array`: returns `[line, newShown]`.
    - earned && shiny && shown < 2 → `[APPEAR_SHINY, 2]`.
    - earned && !shiny && shown < 1 → `[APPEAR_WILD, 1]`.
    - otherwise → `[APPEAR_NONE, shown]`.
  - `function fresh(when as Number or Null, now as Number, maxAge as Number) as Boolean`.
- `Icons`: a new `SAND` enum value, with DUST, SAND, SANDSTORM and HAZE mapped to it.

- [ ] **Step 1: Write the failing tests** in `Tests.mc` (S = SPORT_RUNNING; today = 1792713600; week = today − 3·86400):
  - `testSummaryLatestRun`: two runs today, `[S, today+3600, 1800, 5000]` and `[S, today+7200, 2400, 8200]`, and one run in the week, `[S, week+3600, 3000, 10000]`. With now = today+36000, it gives `[true, 8200, 2400, 23200]`.
  - `testSummaryEmpty`: `summary([], …)` gives `[false, null, null, 0]`.
  - `testSummaryIgnoresOtherSports`: a cycling act with type ≠ S doesn't count.
  - `testSummaryFitEpoch`: a start of today+3600−631065600 still counts as today.
  - `testWeekStartMonday`: Monday (dow 2) gives today; Sunday (dow 1) gives today − 6·86400.
  - `testLevelAndExp`: `level(23200) == 23`; `expFraction(25000) == 0.5`; `expFraction(80000) == 1.0`; `level(0) == 0`.
  - `testPace`: `pace(5000, 1560, true).equals("5:12/km")`; `pace(1609, 502, false).equals("8:22/mi")` (502 s per mile).
  - `testPaceZero`: `pace(0, 600, true)`, `pace(5000, 0, true)` and `pace(null, null, true)` all give `"--"`.
  - `testBadge`: (15, true, false, 90) → PSN; (16, false, false, 90) → PAR; (16, true, true, 25) → SLP; (16, true, false, 25) → NONE; (16, true, true, null) → NONE.
  - `testDexIgnoresLastWeek`: days `[week − 86400, week, week, today]` give 2.
  - `testAppearUpgrade`: `appear(0, true, false)` → `[WILD, 1]`; `appear(1, true, false)` → `[NONE, 1]`; `appear(1, true, true)` → `[SHINY, 2]`; `appear(2, true, true)` → `[NONE, 2]`; `appear(0, false, false)` → `[NONE, 0]`; `appear(0, true, true)` → `[SHINY, 2]`.
  - `testFresh`: `fresh(1000, 1800, 900)` is true; `fresh(1000, 1901, 900)` is false; `fresh(null, 1000, 900)` is false.
  - Extend `testWeatherGroups`: `CONDITION_SAND` and `CONDITION_DUST` → SAND, `CONDITION_FOG` still → FOG.
- [ ] **Step 2: Run `ciqtest.ps1`.** Expected: a compile error or FAIL.
- [ ] **Step 3: Implement `Runs`, the `Partner` additions and the `Icons` SAND change.**
- [ ] **Step 4: Run `ciqtest.ps1`.** Expected: all PASSED.
- [ ] **Step 5: Commit** `feat(garmin): Pokéface logic for runs, weekly level, badges, Pokédex`

### Task 3: Reading the history, cached

**Files:** Modify: `garmin/pokeface/source/Stats.mc`, `garmin/pokeface/source/PokefaceView.mc`

**Interfaces:**
- `Stats.activities() as Array<Array>`: the history as `[type, start, duration, meters]`. Guard with `has :getUserActivityHistory`; null fields become 0.
- `Stats.stepGoalDays() as Array<Number>`: day-start epoch seconds from `ActivityMonitor.getHistory()` where steps ≥ goal > 0, plus today if met. Guard with `has :getHistory`.
- `Stats.heartRate()` uses `Partner.fresh(sample.when.value(), now, 900)` for the history fallback.
- View fields:
  - `_runs` (the summary array);
  - `_dexDays`;
  - `_runsDay` (dayKey of the last read);
  - `_night` (computed in `refresh`).
  - `readRuns()` is called from `onShow`/`onExitSleep`, and from `refresh` when `_runsDay` differs from today.
  - `ranToday()` is replaced by `_runs[0]`.

- [ ] **Step 1: Implement.** `onUpdate` uses `_night` (set in `refresh`) instead of recomputing. Delete the old `ranToday()` walk.
- [ ] **Step 2: Confirm the distance unit.** Add a temporary `System.println` of the first activity's distance and duration, run the simulator after creating a run in its activity data if available (otherwise ledger "unverified, assumed metres per CIQ docs"), then remove the print.
- [ ] **Step 3: Run `ciqtest.ps1`** (no regressions) and `build.ps1 -Run`. Expected: it builds, and the face shows as before.
- [ ] **Step 4: Commit** `feat(garmin): Pokéface reads runs on wake, step-goal days from history`

### Task 4: Drawing it

**Files:** Modify: `garmin/pokeface/source/PokefaceView.mc`, `garmin/pokeface/source/Battle.mc`, `garmin/pokeface/source/Sprite.mc`

**Interfaces:**
- `Sprite.show(species as Number, shiny as Boolean)`: loads `Shiny<i>` when shiny and species % 3 == 2.
- `Battle.badge(dc, x, y, kind as Number) as Number`: draws the tag and returns its width (0 for none).
- `Battle.weather(dc, kind as Number, frame as Number, night as Boolean)`: the particles are deterministic from `frame`, there are at most 30 shapes per frame, and nothing is drawn for NONE/SUN/CLOUD/FOG.

- [ ] **Step 1: Info box.**
  - Lv: `"Lv" + Runs.level(week)`, then the badge right of it.
  - The BB row becomes `DEX n/7` on the left and `BB nn` on the right.
  - EXP is `Runs.expFraction(week)`, and the steps text stays.
- [ ] **Step 2: Dialogue box.**
  - Row 2 shows `Runs.card(...)` when ran today, else distance, keeping the `Fit` 1× fallback and the feet icon.
  - While an "appeared" line is active (set on wake by `Partner.appear`; stored with `Application.Storage` key `"appear"` as `[dayKey, shown]`), both rows are replaced by the line, at 2× if it fits, else 1×, wrapped onto two rows if needed.
- [ ] **Step 3: Sprite.** Call `show(_species, earned && ranToday)`.
- [ ] **Step 4: Weather.** Call `Battle.weather` right after the field when `_timer != null`.
- [ ] **Step 5: Run `ciqtest.ps1` and `build.ps1 -Run`.** Expected: the face renders unchanged when there is no run, badge or weather.
- [ ] **Step 6: Commit** `feat(garmin): Pokéface shows shiny rewards, run card, badges, Pokédex, weather`

### Task 5: Simulator checks and install

**Files:** none (temporary overrides only, reverted)

- [ ] **Step 1: Stage and screenshot each case** by pinning values in `refresh`, the way earlier plans did (`shot.ps1`, PrintWindow):
  - a shiny reward with the "A shiny LATIAS appeared!" line;
  - the run card `8.2 km 5:12/km` with Lv23 and EXP at 46 %;
  - each badge (PSN, PAR, SLP);
  - `DEX 4/7`;
  - rain, snow and sand weather (two frames each, which must differ);
  - the widest case: `12.34 km 10:59/km` plus a PSN badge plus `DEX 7/7`.

  Make a contact sheet and look at it. Expected: nothing clipped and everything readable. Note the simulator's memory peak.
- [ ] **Step 2: Revert the overrides.** Run `ciqtest.ps1`, `python -m unittest discover -s tools -p "test_*.py"` and `build.ps1`.
- [ ] **Step 3: Install** to `GARMIN/APPS` if the watch is plugged in.
- [ ] **Step 4: Commit** any final tweaks as `fix(garmin): …`, and append an "As built" note to the extras spec.
