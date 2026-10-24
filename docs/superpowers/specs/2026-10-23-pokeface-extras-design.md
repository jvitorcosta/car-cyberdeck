# Pokéface extras: shiny rewards, run cards, badges, Pokédex, weather

Builds on `2026-10-23-pokeface-emerald-design.md` (including its "As built" section) and the v1 spec's device, power and licensing rules. Branch `feat/pokeface`.

## Intent and why

João wears Pokéface and likes it. These extras make his **runs** feel like part of the game: a shiny legendary, a "wild … appeared!" moment, the run's numbers on the face, and a weekly level that tracks his running. They also add a few small touches from Emerald (status badges, a Pokédex count, weather in the battle) and clear the four deferred minors.

Deck sync (the decks showing the watch's Pokémon of the day) is **not** part of this. It changes the deck firmware and will get its own session.

## Goals (all agreed with João; guesses he accepted are marked)

1. **Shiny reward.** If a run earned today's reward, the reward legendary is drawn in its **shiny** colours. A reward earned by the step goal is drawn normally. If a run comes later that day, the reward turns shiny *(guess, accepted)*. The shiny colours come from the expansion's `shiny.pal` for each of the 7 reward species. They are fetched, approved and git-ignored like the rest.
2. **"A wild LATIAS appeared!"** The first time the face wakes (`onShow` / `onExitSleep`) after the reward is earned, the dialogue box shows `A wild <NAME> appeared!` in the Emerald font for that wake, about 10 s. A shiny reward shows `A shiny <NAME> appeared!` *(guess, accepted)*. After that wake, the box goes back to the stats. The face shows the line once per earned reward per day, and that state survives the face restarting (store it).
3. **Run card.** After a run, the dialogue box's second row shows **today's latest run**, distance and pace (e.g. `8.2 km 5:12/km`, or `/mi` with statute units), in place of today's total distance until midnight *(guess, accepted)*. With no run today, the row keeps today's distance.
4. **Level as weekly km.** "Lv" shows **this week's running distance in whole km**, from Monday 00:00 local, so a new week starts at Lv0. The blue **EXP bar fills toward 50 km a week** (João's number; one constant, easy to change). The step count stays as a number under the bar ("N steps"). Steps no longer drive the bar.
5. **Status badges.** One Emerald status tag next to "Lv" in the info box, in the game's colours *(guess, accepted)*. Only the most urgent one shows, in this order:
   - **PSN** (purple): watch battery ≤ 15 %.
   - **PAR** (yellow): phone disconnected.
   - **SLP** (grey): at night with Body Battery ≤ 25.
6. **Weekly Pokédex.** `DEX n/7` at the left of the Body Battery row in the info box, with `BB nn` staying on the right *(guess, accepted)*. `n` is the number of days this week (Monday to Sunday) on which the reward was earned. It resets on Monday and must survive restarts (store it by day).
7. **Weather in the battle.** Emerald-style weather moves over the field (above the sky and grass, below the Pokémon's boxes) **only while awake** (the ~10 s animation window), and nothing is drawn asleep:
   - **rain** streaks for the rain and storm groups;
   - **snowflakes** for the snow group;
   - a **sandstorm** for dust, sand and haze. Today `Icons.group` puts those in FOG along with fog and mist, so split out the sand ones.
   - nothing for sun, cloud or fog.
8. **The four deferred minors:**
   - **Sky and Pokémon:** they switch at sunrise/sunset in the same minute. Compute day/night once in `refresh` and use it for both.
   - **Heart rate:** a history sample older than 15 minutes shows as `--`.
   - **Run check:** once a run is found today, stop walking the activity history until midnight (cache it).
   - **Sprite tool:** `colourise` raises a clear error for a source that isn't palette-indexed, or whose index is beyond its palette.

## Calls made after the builder check (Claude's, cheap to change)

- **When the run data is read** (resolves fix 3 against goals 1, 3 and 4): the activity history (today's runs, the latest run, the week's km) is read **when the face appears or wakes** (`onShow` / `onExitSleep`, which also covers coming back from an activity) and at the first update after midnight. The once-a-minute refresh uses the cached numbers. A second run that day therefore shows up as soon as you look at the watch after it.
- **A shiny upgrade gets its own line:** if the step goal already showed "A wild X appeared!" and a run later makes the reward shiny, the next wake shows "A shiny X appeared!" once. The stored state per day goes none → normal shown → shiny shown.
- **The Pokédex is worked out from the watch's history, not stored.** Run days come from the activity history. Step-goal days come from `ActivityMonitor.getHistory()`, which keeps about 7 days of steps and goals, plus today. Days before the face was installed, or spent on another face, still count, and there's nothing to reset on Monday beyond the date maths.
- **The level is always in km,** with a 50 km goal, even on a watch set to miles. That's what João picked. The run card follows the watch's units.

## Non-goals

Deck sync, settings in Garmin Connect, new layouts, new Pokémon.

## Anti-goals

- Worse battery than now. Everything new reads once a minute and on wake, and weather and text animate only while awake.
- Anything clipped by the round edge. Use `Fit`, dropping to 1× like the existing rows.
- Art committed to the public repo.
- A crash when activity history, weather or distance is missing.

## Constraints

- fr255, CIQ 5.2, 128 KB watch-face memory (about 20 KB in use now).
- Sprites are drawn from 128×64 sheets at 2×. A shiny variant needs its own sheet (`mon<i>s.png`) or a palette swap. Only the current one is loaded.
- Activity history: `UserProfile.getUserActivityHistory()`, whose entries have type, startTime, duration and distance. The builder confirms the distance unit and `startTime` epoch in the simulator; v1's `finishedToday` FIT-epoch rule stays.
- The ledger and rulings process from the earlier plans continues.

## Left to the builder

- Exact badge drawing (size, rounded tag), weather particle counts and speeds, pace formatting details, and storage keys.
- How the "appeared!" state and the weekly DEX are stored, as long as they survive a restart and reset correctly at midnight / on Monday.
- Pure functions plus unit tests for: weekly km and its bar fraction, pace, which badge shows, DEX counting and its Monday reset, and when the "appeared!" line shows.

## As built (2026-10-23)

- **Status tag:** it sits at the left of the steps row in the info box, not beside "Lv". The Lv row holds the HP bar, and in the DEX/BB row the tag ran into "BB nn".
- **Run card:** "8.2 km 5:13/km" only fits at 1× on the dialogue's lower row (about 170 px at 2×, where the round edge leaves about 116), so it shows at 1×.
- **"appeared!" line:** "A wild / shiny NAME" and "appeared!" each take a row, at 2× where `Fit` allows, otherwise 1×. The font gained "!".
- **Weather:** 60 particles while awake: white 2 px rain streaks, 3×3 snow, 10×2 orange sand dashes.
- **Pokédex:** each day value is bucketed to the nearest local midnight counted back from today, so the step history's UTC-midnight stamps and the runs' local midnights agree.
- **Run distances:** turned into whole metres.
- **Memory:** about 24 KB of 124 KB in the simulator.
- **Unverified until a real run on the watch:** the unit of `UserActivity.distance` (assumed metres) and whether `History.startOfDay` is local or UTC midnight. The simulator has no activity history.
