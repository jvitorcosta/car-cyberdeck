# Yellow deck: terminal menu, night mode, favourites (design)

> **Status:** the menu and night mode were built (#18); the four saved favourites were superseded by
> six fixed presets (#20), and the Car and Heart pages were not built. See `docs/yellow-deck.md`.

Date: 2026-10-23. Builds on the LED panel (`src/ledpanel.cpp`, env `ledpanel`, merged in #13) and
the Garmin "Honda City" app (`garmin/carlights/`). Voice and the watch are documented in
`docs/leds-voice.md`.

## Intent

The yellow deck (the CYD, 320 x 240, resistive touch) has become the car's control screen for the
LED lights. The owner wants it to grow into a small **car computer** with a main menu: Car,
Lights, Weather, Heart and later Music. It should **look like a Fallout RobCo terminal**: green
phosphor on black, monospace text, `[ BRACKETED ]` options with the selected one drawn inverted.
The gauge cluster on `feat/obd` (spec `2026-10-19-gauge-cluster-design.md` on that branch) was
already designed in that same green, so the whole deck ends up in one style.

Three things motivate the first part of the work:

- **The screen is too bright at night.** The black deck already dims at 18:00. The yellow deck
  should do the same.
- **Favourites.** The owner wants 4 saved setups within easy reach, each covering the whole car,
  for example "pink, inside 50 %, outside 100 %".
- **An "on" indicator.** A clear sign of whether each light is on and whether the deck is actually
  connected to it.

The Garmin app should reflect favourites and the on status as well.

## The work, in order

This is several projects. They share one shell (the menu and the theme), so the shell comes
first. Each later part gets its own plan once the earlier one is in the car.

1. **Menu shell, terminal theme, night mode, Lights rework (Back, on indicator, favourites).**
   Detailed below. It is the subject of the first plan.
2. **Garmin: favourites and the on status.** Detailed below, second plan.
3. **Weather screen.** Outlined below.
4. **Car screen**: the `feat/obd` gauge cluster, over **BLE** next to the lights. Blocked until the
   Vgate iCar Pro BLE 4.0 adapter arrives (ordered from China). Outlined below.
5. **Heart screen**: heart rate from the watch. Outlined below.
6. **Music screen**: paused in its own brainstorm. The black deck's mic still has to prove it can
   hear music (an 8-band test is pending). Not designed here.
7. **Bonus: LED effects** (fade, jump, strobe, breathe) as a pattern in a favourite. Only after 1-2,
   and only what the ELK controllers turn out to support.

## Goals (parts 1 and 2)

- A **main menu** on the yellow deck as the home screen. Touch and the Xbox D-pad both navigate it.
- The **terminal look** on the menu and on the Lights page. Colour tiles and the light previews stay
  in real colour, since they *are* the colours, inside the green chrome.
- **Night mode** from 18:00 to 06:00 local time: a dimmer backlight and a dimmer palette.
- **Lights page**: the current panel (zones BOTH/INSIDE/OUTSIDE, 8 colours, `-` level `+`,
  double-tap to switch off), plus a **Back** button, an **on/link line** and **4 favourites**.
- **Favourites** reachable from touch, the Xbox controller, voice and the watch.
- Siri and the Garmin app **keep working** on whatever screen is showing.

## Non-goals (parts 1 and 2)

- Weather, Car, Heart, Music screens (later parts).
- LED effects or patterns (bonus, later).
- Automatic dimming from the CYD's light sensor. The schedule is enough, as on the black deck.
- A manual day/night switch (not asked for. It can be added later if the schedule annoys).
- Saving favourites from the watch or from voice. They are set up on the deck.
- Merging the yellow deck back into the `cyd_companion` firmware. The menu grows inside the
  `ledpanel` build.

## Anti-goals (a failure even if it works)

- **The lights get worse.** Any regression in how fast or reliably a tap, a voice command or a
  watch press changes the lights. The menu is a wrapper around the panel, not a rewrite of it.
- **A menu you have to fight while driving.** Buttons too small for a finger, more than one tap
  from the menu to a favourite's effect, or a screen that redraws slowly enough to notice.
- **Glare at night**: a white or bright-green full-screen flash at night, for example a full
  repaint in the day palette before night mode applies.
- **A fake on indicator**: showing "ON" for a light the deck isn't connected to.

## Constraints

- **Board**: ESP32 (CYD), 4 MB flash, ILI9341 320 x 240 over TFT_eSPI, XPT2046 touch with the
  homography calibration. The backlight is on `PIN_BL` (GPIO 21), as in `ui/backlight`.
- **Bluetooth**: NimBLE 2.5.1 (lights + Xbox). Bluetooth Classic is not available in this build.
- **Memory**: watch the `[mem]` line. WiFi, NimBLE with 3 connections, the web server and the
  Xbox library already share the heap. Part 4 adds a 4th BLE connection.
- **Network**: the deck lives on the iPhone hotspot at a fixed `172.20.10.11`
  (`CARDECK_COMPANION`). The hotspot has internet, so SNTP works there. On home WiFi it may work
  too.
- **Voice** stays English (Siri in English, Shortcut "Honda").
- **Public repo**: no tokens in code (`REMOTE_TOKEN` stays in `src/config.h`, the watch key in the
  git-ignored `Secrets.mc`), no personal data.
- **Garmin**: Forerunner 255 family, Connect IQ 9.3, round 260 x 260 MIP, 8 bpp. Plain http over
  the hotspot is proven.

## Part 1: the yellow deck

### Screens and navigation

```
MENU ──A/tap──► LIGHTS ──Back/B──► MENU
  │                (WEATHER, CAR, HEART, MUSIC later, same pattern)
```

- **Main menu**: a header (`ROBCO INDUSTRIES (TM) TERMLINK`, then a second line such as
  `HONDA CITY // CARDECK OS`), a grid of large bracketed buttons, and a bottom status strip.
  Approved direction (mockup shown in the session): a 2-column grid of buttons about 140 x 36 px
  each. **Builder's call:** the exact sizes. Every button must be hittable with a finger: the
  layout D tiles are the reference.
- **Only built screens appear** in the menu. In part 1 that's just **Lights**, and the grid fills as
  later parts land. *Builder's call (mine)*: no greyed-out placeholders, since a dead button in a
  car is worse than a missing one.
- **Boot** lands on the main menu.
- **Favourites on the menu too** (*my call, the owner said "continue" without choosing*): a row of
  F1-F4 on the main menu, the same previews as on the Lights page, so a favourite is **one tap
  from boot**. On the menu they only apply. Saving (long-press) happens on the Lights page. The
  Xbox bar moves through the buttons and the slots alike.
- **Status strip** (bottom of the menu): the clock (once SNTP has synced), and the **on indicator**
  for both lights: `IN ON` / `OUT ON` in the light's current colour, `OFF` in dim green, and a
  clear "no link" state (for example `IN --`) when the deck isn't connected to that controller.
  Weather slots in here in part 3.
- **Xbox**: the D-pad moves the inverted bar, **A** opens, **B** goes back to the menu from any
  screen. On the menu, B does nothing. The left stick stays ignored (drift). *My call*: B used
  to jump to the BOTH page. It becomes Back. BOTH is still reachable with X (next page), and Y
  (on/off) and LB/RB (level) keep their jobs. On a focused favourite slot, A applies on
  **release** so that holding it can save.
- **Touch**: a tap opens a menu button. Every screen other than the menu has a **`< BACK`** button
  top left.

### The terminal theme

- **Palette**: one green in a few brightness steps on black, for example bright for text and
  selected, mid for borders, dim for hints. Plus the real colours for tiles and light previews.
  **Builder's call:** the exact greens, tuned on the panel (it washes colours out, as found with
  layout D).
- **Font**: monospace. TFT_eSPI's built-in fonts or a small custom GFX font. **Builder's call**, as
  long as it reads as a terminal and stays readable at arm's length.
- **Selected item**: an inverted bar (green background, black text), the RobCo convention. The
  Xbox focus ring becomes that bar on the menu. On the Lights page the focus stays a ring around
  the tile, in bright green rather than today's cyan.
- **No scanlines, glow or flicker** in this version (cost and readability). They can be a later
  polish.

> **2026-10-23 (owner): the terminal look goes further, superseding "No scanlines" above.** Faint
> scanlines, ALL CAPS bracket labels, a `> [LIGHTS]` row, Pip-Boy tabs for the zones, bordered colour
> boxes with a big colour chip, a 10-cell level bar (%, MIX or OFF), a typed RobCo boot (~0.5 s) and a
> blinking block cursor on the Menu strip. Status messages (`>ROSE APPLIED`, `>INSIDE: NO LINK`,
> `>OUTSIDE: LINKED`) replace the strip's text for 2.5 s. The strip shows the real colours
> (`IN PINK 40%  OUT PINK 100%`; `OFF`, `--`).

### Night mode

- Same rule and code as the black deck: `util/daynight` (18:00-06:00), `net/clock` (SNTP, the
  `TIMEZONE` in `config.h`), `ui/backlight` (day 255, night 90 ≈ 35 %, tune on the panel).
  The `ledpanel` build gains those files.
- At night: the backlight dims **and** the greens step down. The light colour tiles also dim a
  bit (**builder's call**: how much, so they stay recognisable).
- Checked about once a minute. Switching repaints the screen in the new palette **without a bright
  flash** (see anti-goals).
- Until the clock syncs, the deck stays in day mode, as the black deck does. *Builder's call*: if
  that looks wrong at night in practice (booting with no hotspot), revisit. The deck gets the
  hotspot within seconds in the car, so it shouldn't matter.
- Night mode changes the **screen only**, never the car's LEDs.

> **2026-10-23 (owner): amber at night.** The night palette is amber on black instead of dimmed green
> (nothing stays green at night); the backlight rule is unchanged.

### The Lights page

Everything the current panel does stays: the BOTH / INSIDE / OUTSIDE pages, 8 colours,
`-` / level / `+`, double-tap the lit colour for off, the debounced NVS saves, the per-zone looks.
The layout has to make room for:

- **`< BACK`**, top left.
- **The on/link line**: for each light, whether it's lit, at what level, and whether the deck is
  connected, for example `IN ON 50%  OUT ON 100%  LINK 2/2`. Connected and lit are separate
  facts. A light the deck isn't connected to never shows as ON (anti-goals). *My call*: "lit"
  means what the deck last sent. The ELK controllers can't be read back over FFF3, and the deck
  re-sends its state on every reconnect, so the two agree. A change made from outside (the iPhone
  app, a controller's own remote) isn't seen. That's acceptable, because the deck is the only
  remote in use (the iPhone's Bluetooth stays off in the car).
- **4 favourite slots** (`F1`..`F4`). Each shows a small preview: the inside and outside colours
  (dots or squares) and enough to tell the levels apart. An empty slot shows `--`.
  - **Tap** a slot to apply it.
  - **Long-press** (*builder's call*: ~800 ms, with visible feedback when the save happens) to save
    the current look of **both** lights into it, replacing what was there.
  - **Xbox**: A applies the focused slot. *Builder's call (mine)*: hold A ~1 s to save, mirroring
    the long-press.
- **Builder's call:** the arrangement. The mockup from the session put zones top right, the on line
  under the header, the colour grid 4 x 2, the favourites in a row, and `-` level `+` at the bottom.
  The colour tiles may shrink from layout D to fit, but must stay finger-sized.

### Favourites

- A favourite = **both lights' looks**: for inside and for outside, colour, level and on/off. The
  same `Look` the deck already keeps per zone.
- Applying a favourite sets `looks[INSIDE]` and `looks[OUTSIDE]`, pushes both lights, saves, and
  leaves the page on whatever zone was showing. *Builder's call*: what BOTH's look becomes after
  applying a favourite whose two sides differ. The BOTH page needs a sensible display for "mixed".
- Stored in NVS next to the looks, surviving restarts. Empty slots by default.
- **Voice**: "favourite one" ... "favourite four" (also "favorite", "preset", and digits) applies
  that slot. It replies with what it applied, for example "Favourite 1: inside Pink 50 percent,
  outside Pink 100 percent", or "Favourite 3 is empty". English Siri mishears words, so the parser
  should take the likely mishearings, as it already does for colours. **Builder's call:** which
  ones, based on tests with the Shortcut.

> **2026-10-23 (owner): fixed named presets replace saved favourites.** The four slots are no longer
> saved from the deck (no hold-to-save, no NVS slots, no empty slots). They are fixed, both lights on:
>
> | # | Name | Inside | Outside |
> |---|---|---|---|
> | 1 | ROSE | pink 40 % | pink 100 % |
> | 2 | FIRE | red 50 % | red 100 % |
> | 3 | CALM | white 40 % | white 40 % |
> | 4 | PURPURA | purple 40 % | purple 100 % |
>
> Voice: "rose", "fire", "calm", "purpura" (the owner's spelling, the one non-English name) or
> "preset/favourite + one..four / 1..4"; a preset ignores the rest of the phrase, and the reply is e.g.
> "Rose: inside Pink 40 percent, outside Pink 100 percent". `GET /presets?k=` replaces `/favs`
> (`1 ROSE pink 40 pink 100`, four lines). The watch's fourth page is PRESETS (names and two dots).
> Presets are the Menu's main element (four blocks under the LIGHTS row).

### The deck's HTTP API

Today: `GET /say?k=&text=[&zone=]` and `GET /state?k=[&zone=]`. Parts 1 and 2 need the watch to
read the favourites and the link state. **Builder's call:** the exact endpoints and formats, as
long as:

- `/say` and `/state` keep their current behaviour (the Siri Shortcut and the watch depend on it).
- The watch can list the 4 favourites (empty or a short description each) and apply one.
  Applying through `/say?text=favourite 2` is fine.
- The watch can learn, per light, **lit or not, and connected or not**.

### Part 1 is done when

- The deck boots to the terminal menu and opens Lights from touch and from the Xbox controller.
  Back returns from touch and from B.
- From 18:00 the screen is dim and stays readable, with no flash at the switch.
- Favourites save, apply and survive a restart. Voice applies them.
- The on/link line matches reality: switch a controller off (or walk out of range) and it shows
  disconnected within a few seconds.
- Siri and the watch still change the lights on any screen.
- Tested in the car: lights, voice, the Xbox controller, at night.

## Part 2: the Garmin app

- A **Favourites** page in the UP/DOWN page loop (*builder's call*: after OUTSIDE, so BOTH stays
  first). It shows F1-F4 with colour previews like the deck's (the `ColorDot` style). *My call
  (option A)*: START opens a Menu2 list of F1-F4 and selecting one applies it. That's two presses,
  but UP/DOWN keep turning pages everywhere, like START on the zone pages today. Empty slots are
  shown and can't be applied.
- The **on status**: the BOTH / INSIDE / OUTSIDE pages show when a light is off (they already do)
  **and** when the deck isn't connected to it (new), in the existing pill on the bottom line.
  **Builder's call:** the wording.
- The glance (`HondaGlance`) may show the on status too. *Builder's call*, if it fits.
- Done when: favourites apply from the watch and the pages show a disconnected light, tested
  against the deck over the hotspot.

## Part 3: Weather (outline)

`util/weather` (Open-Meteo, free, no key) is already in the repo, unused. A Weather screen shows
the current temperature and conditions, and the menu's status strip shows the temperature.
Open questions for its own plan: the location (a fixed city in `config.h`, or something smarter),
https versus http (TLS costs heap next to NimBLE), and the refresh rate.

## Part 4: Car (outline)

The `feat/obd` gauge cluster (already Fallout green) on the Car screen, with the ELM327 over **BLE**
(`util/obdble` exists on that branch) as a 4th BLE connection next to the two lights and the Xbox.
Steps: raise NimBLE's connection limit (3 by default), then prove 4 connections and the heap in the
car with the new Vgate iCar Pro BLE 4.0. If that holds, fold the gauges in. Before the adapter
arrives, the current dual-mode adapter's BLE side ("OBDBLE") can be tried with NimBLE 2. It was
flaky under the old library. `feat/obd` itself is still unmerged until the owner validates its
readings.

## Part 5: Heart (outline)

The watch sends the heart rate to the deck, and a Heart screen shows it big, maybe with a short
graph. Limit: Garmin lets an app send it every few seconds **only while the Honda City app is
open** on the watch. In the background it's about every 5 minutes. So it's a live screen while
the app is open, and shows the last value with its age otherwise. Its own plan decides whether
that's worth it.

## Decided in the conversation (the owner's)

- A main menu on the yellow deck: Car, Lights, Weather, Heart (if possible), Music later.
- The Fallout / RobCo terminal look.
- Night mode like the black deck's: 18:00, lower intensity.
- 4 favourites, each a whole-car setup (per-light colour and level).
- An on indicator, also on the watch. Favourites on the watch.
- Effects (spiral, chase) only as a bonus after the main features, and only if possible.
- Car = OBD, made possible over BLE with the Vgate iCar Pro BLE 4.0 on its way.
- Garmin and Siri keep working. The Xbox controller navigates the menu and the Lights page, with
  Back.

## Left to the builder

Marked **builder's call** above: exact sizes, layout of the Lights page, the greens and fonts,
how much the tiles dim at night, the long-press time and feedback, the "mixed" BOTH display, the
voice mishearings, the HTTP formats, and the watch page wording and order.
