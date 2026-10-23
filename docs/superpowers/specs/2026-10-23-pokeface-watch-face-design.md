# Pokéface: a Charmander-line watch face for the Forerunner 255

## Intent

João wants a watch face of his own for his Garmin Forerunner 255 that matches the fun of the deck projects in this repo. It has a Pokémon partner that lives on the wrist and reacts to his day. It's the Charmander line in the real animated sprites, and it Mega-evolves when he has earned it (a run or his step goal). It's also a practical face that shows everything he glances at during the day.

It's a personal face, sideloaded from this PC like the Honda City app (`garmin/carlights/`). It never goes on the Connect IQ Store.

## Goals

- A Connect IQ **watch face** for fr255 / fr255m, in `garmin/pokeface/`.
- The partner changes with the time of day and becomes a Mega when earned.
- The sprite animates briefly when he raises his wrist.
- All the data below is visible at once (no paging or rotating lines).

## Non-goals

- Store publishing, settings menus, or user-pickable Pokémon. The line is fixed to Charmander, Charmeleon, Charizard and Mega X/Y.
- Talking to the deck, Spotify or the car LEDs.
- Recovery time, training readiness, stress, intensity minutes, floors, seconds, alarms, DND. He chose not to have them.

## Anti-goals (a failure even if it works)

- A face that noticeably shortens battery life compared with a stock Garmin face.
- Text too small to read at a glance on the 255's screen.
- Sprites that look smeared or wrongly coloured (no blurry or non-integer scaling, and the palette must survive the 255's 64-colour display).
- Nintendo sprite files committed to this **public** repo.

## Constraints

- Device: João wears the **Forerunner 255** (carlights builds with `-d fr255`): a 260×260 round MIP display, 64 colours, buttons only (no touch), so a watch face can't be tapped. The face targets fr255 and fr255m (same screen). *(Claude's call: the 255S/255S Music have a 218×218 screen and would need their own layout, so they're left out.)* Watch-face memory limit is 128 KB.
- The SDK (9.3.0), device files and developer key are already set up (see `garmin/carlights/`). Build with `monkeyc` for `fr255` and install by copying the `.prg` to `GARMIN/APPS` over MTP, the same as carlights.
- Watch face power model: a minute-by-minute update while asleep, plus a high-power window of about 10 s after a wrist raise. Animation happens only in that window.
- Memory on a watch face is tight. Sprite frames must be trimmed to fit, and the builder measures the real limit for fr255.
- **Sprite licensing:** the official sprites are Nintendo/Game Freak/Creatures property. Keep them out of git. Commit a fetch/convert script and git-ignore the images it produces (the same pattern as the `data/*.bin` assets in `.gitignore`). Downloading needs João's OK on the exact source URLs and files first.

## Decided by João

### Sprites
- Charmander, Charmeleon and Charizard use the official **Pokémon Black/White animated sprites** (front, 96×96 class).
- Mega Charizard X and Mega Charizard Y use **fan-made Black/White-style animated sprites** from the Smogon sprite project (the ones Pokémon Showdown uses). No official pixel versions exist.
- Each animation is cut to its best handful of frames to fit memory.
- **Shown at half size (João's call):** each 96×96 frame is scaled down 2× with nearest-neighbour sampling to 48×48, so it's blocky rather than blurred. This keeps the big time and all the data on screen. The trimmed, half-size frames also make the 128 KB memory limit much easier to meet.

### Who appears when
| Time | Partner |
|---|---|
| 06:00–12:00 | Charmander |
| 12:00–18:00 | Charmeleon |
| 18:00–22:00 | Charizard |
| 22:00–06:00 | Asleep: Charizard (or the day's Mega until midnight), darkened, held still, with a small "Zzz" and a dim tail flame |

*(Claude's call on night: the sleeper is Charizard. A Mega earned that day sleeps as that Mega until midnight, then the day resets to Charizard. Sleeping uses one darkened frame per Pokémon with no animation frames. A wrist raise at night only nudges the "Zzz" and doesn't play the idle animation, and the minute-by-minute pose change doesn't apply while asleep.)*

The hours are Claude's guess, accepted by João without changes.

**Mega:** after a **run is finished today** or **today's step goal is reached**, the partner becomes Mega Charizard X or Y, chosen **at random**, and stays that Mega for the rest of the calendar day. *(Claude's call: the choice is made once when it's earned and stays fixed for the day, not re-rolled every redraw. The Mega also sleeps at night. The next day starts fresh.)*

### Layout: "A, all at once"
Partner front and centre, everything visible together (top to bottom):
1. Weather condition icon, today's high/low, and the next sunrise or sunset (whichever comes next)
2. Weekday and date, a phone-connected dot, notification count, battery %
3. Time, large
4. The Pokémon, centred. Heart rate and temperature on its left, Body Battery and calories on its right
5. Step progress bar against the step goal, then steps and distance today

The mockup approved in brainstorming is `.superpowers/brainstorm/19703-1792765866/content/layout-a-v3.html` (option "All at once"). It is git-ignored and local only.

### Data
- Heart rate: current/most recent HR. It refreshes once a minute while asleep and more often in the high-power window.
- Temperature: **outdoor temperature from the phone-synced weather** *(Claude's call: the wrist sensor reads body heat)*. The icon and high/low come from the same weather data.
- Body Battery: shown as a number **and** it scales the tail flame on the sprite, so a full battery means a big, bright flame and a drained one a small flicker *(Claude's call)*.
- Calories, distance, steps, step goal, notifications, phone connection, battery: what the device reports for today.

### Movement
- Asleep: one pose that changes once a minute so it never looks frozen.
- Wrist raise: plays the trimmed idle animation for the high-power window, then goes back to the still pose.

## Left to the builder

- Exact pixel positions, fonts and icon art (within the mockup's arrangement), and the colours for text and the HR/BB accents.
- How to animate: `onUpdate` timers during high power or `WatchUi.AnimationLayer`. Pick whatever fr255 supports and stays inside the power budget.
- Cropping empty padding around the half-size frames, frame count per Pokémon, and how frames are packed (bitmap resources, sprite sheets) to fit memory.
- How the flame is drawn (overlay on the sprite vs separate flame frames) and how BB maps to its size.
- How a finished run is detected (e.g. `UserProfile.getUserActivityHistory()` filtered to running today) and how the day's Mega choice is stored (`Application.Storage`).
- What to show when a value isn't available (no weather sync, phone disconnected, no HR): a dash, never a crash.
- Whether to add a simulator mode that steps through the stages, like `-D OTA_DEMO` on the deck.
