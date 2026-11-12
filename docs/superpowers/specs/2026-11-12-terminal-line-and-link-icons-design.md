# Terminal prompt line, smaller main-screen buttons, GPS and deck-link icons

Date: 2026-11-12. Status: approved in brainstorming (layout B, bottom prompt line, text tags on
the yellow deck, pixel icon on the black deck).

## Why

- **Prompt line.** João likes the yellow deck's prompt line (`>` and a block cursor: `>BLUE`,
  `>LIGHTS OFF`, errors in red), but it moves around today. On the main screen it sits mid-screen,
  and on Lights the messages take the status line under the tabs. A terminal keeps its prompt in one
  place, so the line should sit at the same spot on every screen.
- **Main-screen buttons.** The three tall buttons (`[LIGHTS]`, `KARAOKE`, `MUSIC MODE`) fill the
  bottom half. João uses all three about equally, so all three stay, but they should take less room.
- **Status icons.** Neither deck can tell whether the black deck has GPS, and without GPS the radar
  alerts stop silently. Neither deck can tell whether the two decks are talking to each other. João
  only wants to know "GPS working or not" and "decks linked or not".

## Goals

1. The prompt line is the bottom row of every yellow-deck screen. That covers the main screen,
   Lights on BOTH, INSIDE and OUTSIDE, PRESETS, and touch calibration, always at the same y.
2. The main screen uses layout B: the lights panel is itself the button that opens Lights, and
   KARAOKE and MUSIC MODE are two wide switches.
3. Both decks show a GPS state and a deck-link state, each in its own deck's design language.

## Non-goals

- The boot screen keeps typing its lines down from the top, as it does now. That is a log, not the
  prompt. (My call.)
- No song, lyrics or radar data on the yellow deck beyond the GPS state.
- No change to Lights' colours, presets, level logic, the PS3 shortcuts or the IR remote.
- No light-sensor dimming or other look changes outside these screens.

## Anti-goals

- **Hard to hit while driving.** KARAOKE and MUSIC MODE must stay big targets of at least ~60 px
  tall, and the lights panel must be obviously tappable.
- **Eating the yellow deck's heap.** The lowest free heap online is ~37 KB, and the flash slot is
  86.7 % full. The link check adds no new task and no TLS.
- **A noisy screen.** Icons that flicker on every missed poll, or messages that repeat.

## The yellow deck

Look as today: Seegson green (`util/common/termpal.h`, three levels plus ALERT red), VT323
(`ui/termfont`), scanlines, all caps. Coordinates are on the 320x240 screen. Exact pixel values are
the builder's to tune, and `test_termfont` must keep proving every fixed label fits.

### The prompt line, on every screen

- A MID rule across the screen at about y 220, then on the bottom ~20 px: `>` and the text in SMALL
  at x `DASH_X`, then the block cursor after the text, as the prompt line draws today.
- The content is the same as today's prompt line: a message for 2.5 s (6 s for the boot's last-trip
  line), with errors and NO LINK in ALERT red.
- When no message is showing, each screen shows its own idle text:
  - **Main screen:** music mode's line (`>MUSIC ■ PINK`, `>MUSIC: WAITING`, `>MUSIC: NO SIGNAL`)
    while music mode is on, else `>`.
  - **Lights (BOTH, INSIDE, OUTSIDE) and PRESETS:** what the status line shows today: `>LINK 2/2`,
    or in red `>OUTSIDE: NO LINK`, `>INSIDE: NO LINK` or `>NO LINK`.
  - **Calibration:** its instruction (see below).
- One prompt-line drawer serves every screen, so it can't drift. Its geometry lives in
  `ui/termlayout.h` (for example `PROMPT_RULE_Y` and `PROMPT_BASE`) and its band in `deckmenu` (a
  display-only `PROMPT_BAND` at the bottom, the same on every screen).

### Main screen (layout B), top to bottom

1. **Header**, as today: `HONDA CITY TOURING`, and on the right the PAD battery while a pad is
   connected. The two new tags go just left of PAD (see Icons). When no pad is connected, the tags
   right-align to `DASH_RIGHT`.
2. **Clock row**, as today: the clock, the date, `TRIP` and the trip's time.
3. **The lights panel**, which is also the LIGHTS button:
   - It is one outlined rect, about y 58 to 144 and the full width (x 4 to 315), with a vertical MID
     rule in the middle.
   - Each half has a bigger colour block than today (about 34x52) and its label `INSIDE` /
     `OUTSIDE` (SMALL, MID), with the value under it in TITLE (`ROSE 40%`, `OFF` dim with an
     outlined block, `-- NO LINK` red with no block).
   - A small dim `LIGHTS >` hint sits at the panel's bottom right.
   - A tap anywhere on the panel opens Lights, on the page last shown, as `[LIGHTS]` does today. It
     gets the same pressed look as the other buttons (the solid bar with a ring cut in), and the
     same PS3 focus (the inverted bar).
4. **Two switches**, KARAOKE on the left and MUSIC MODE on the right, each about 152x64 at about
   y 150 to 214. Each keeps its inverted `ON` / `OFF` tag at the bottom right.
5. **The prompt line.**

PS3 D-pad on the main screen: three targets. From the panel, down goes to KARAOKE. From either
switch, up goes to the panel, and left / right moves between the switches. `deckmenu::neighbour`'s
centre rules likely already give this; tests should pin it down. Cross presses, as today.

Touch: `deckmenu::rect(Menu, LIGHTS)` becomes the panel's rect, so the panel stops being a
display-only band. The KARAOKE and MUSIC rects become the two switches. The rule that a tap in the
gap snaps to the nearest item within 10 px stays. The header, clock row and prompt line stay
display only.

`docs/yellow-deck.md` calls the main screen "the Menu"; `deckmenu::Screen::Menu`, `Item` and
`MenuId` stay with their meanings, and only their rects change.

### Lights and PRESETS

- The status line under the tabs (`deckmenu::STATUS`) goes. Its text moves to the prompt line's
  idle text (above).
- `< BACK` and the four tabs stay at y 2 to 38.
- The 2x4 colour tiles move up to start at about y 42. They can get a little shorter (about 62 px),
  so the `-` / level / `+` row (about 40 px tall) ends above the prompt rule at about y 214.
- The six presets on PRESETS shift to fit between the tabs and the prompt rule, two rows of three,
  each still ≥ 60 px tall.
- Messages that showed in the status line (`>ROSE APPLIED`, `>INSIDE: NO LINK`, ...) now show on
  the prompt line.

### Touch calibration

- It moves to the terminal look: a black screen with scanlines, the title `TOUCH CALIBRATION` in
  TITLE, crosses in TEXT, the cross waiting to be tapped in ALERT red, and a tapped one in TEXT.
- Its words go on the prompt line: `>TAP EACH CROSS WITH THE PEN`, then `>CALIBRATED`, or a red
  `>MISSED A CROSS - AGAIN`.
- The two bottom crosses move up from y 216 to about y 196 so they clear the prompt rule. The
  corner crosses only need to be four well-spread points. The homography stored in NVS maps raw
  readings to screen pixels, so a saved calibration stays valid. (My call.)
- `waitTap`, the fit, the 25 px check and the NVS keys don't change.

## Icons

### What they mean

- **GPS:** the black deck has a GPS fix from the iPhone (GPS 2 IP). That is the same truth the black
  deck's existing pin shows: `cameras::Stage::NoFix` means no fix, and every other stage means a
  fix.
- **Deck link:** the two decks are talking. Each deck counts the link as up while it has had a
  valid exchange with the other within the last **15 s**. That is about three of the yellow deck's
  5 s checks, so one missed poll (the black deck's background WiFi scan drops about one a minute)
  doesn't flicker it. This matches `deckaddr::MISSES = 3`.

### The link check (both decks)

- **Black deck:** a new signed `GET /link` on its web server (`net/remote`), authenticated like
  `/music` and `/karaoke` (`reqauth`: `k=` or a signature). It answers with a tiny body that carries
  the GPS state, for example `gps=1` with a fix and `gps=0` without one; the builder picks the exact
  format and parses it in a pure, host-tested helper. A valid signed `/link`, `/music` or
  `/karaoke` records "yellow deck last heard" with its time. `/playlists/play` does **not** count:
  the Garmin watch's playlist picker sends it too, signed the same way. (My call; no sender marker,
  so the watch's code path is untouched.)
- **Yellow deck:** the existing `deck` task (no new task) sends `GET /link` through `deckGet`
  **every 5 s, regardless** of the other asks, so the GPS state never goes stale while music mode
  or karaoke keep the link busy. It is one small plain-HTTP GET; the `/music` and `/karaoke` reply
  formats don't change. (My call.) A valid answer to any ask (`/link`, the keep-alives, playlist
  asks) counts as link evidence; only `/link`'s answer updates the GPS state.
- **Linked:** the last valid answer is less than 15 s old. A pure helper (for example
  `util/common/linkstate.h`, used by both decks) takes last-heard times and `now` and returns the
  link state. It is host-tested, including `millis()` wrap.
- The yellow deck only knows GPS through the black deck. With no link, its GPS state is
  **unknown**.

### Yellow deck: text tags in the header (choice 1)

- `GPS` and `DECK` in SMALL, in the header row, right-aligned just left of the PAD part (or at
  `DASH_RIGHT` with no pad), with a small gap between them.
- States:
  - `GPS` is MID with a fix, red and struck through without a fix, and DIM when unknown (no link).
  - `DECK` is MID when linked, red and struck through when not.
- They show on the main screen only, since Lights has no header. The prompt line's messages reach
  every screen (below).
- **Messages (my call):** when a state changes and the new state holds (debounced by the 15 s
  window for the link, and by the black deck's own fix logic for GPS), the prompt line shows:
  - `>DECK: LINKED`, or in red `>DECK: NO LINK`;
  - `>GPS: FIX`, or in red `>GPS: NO FIX`.

  This way a drop is noticed on Lights too. Nothing shows at boot until the first state is known,
  and the first "linked" after boot is silent.
- **No double messages (my call):** today an outage already shows as `>BLACK DECK NOT ANSWERING`
  (karaoke's confirm, an IR playlist key) or `>MUSIC: NO SIGNAL`. `>DECK: NO LINK` is skipped
  when `>BLACK DECK NOT ANSWERING` showed in the last 15 s. The `DECK` tag turns red either way.
  `>MUSIC: NO SIGNAL` is music mode's idle line, not a message, so it doesn't block it.
- Layout room: "HONDA CITY TOURING" ends around x 110, and `GPS DECK PAD [battery]` needs about
  130 px from the right, which fits. `termlayout` gets the widths and positions, and
  `test_termfont` checks they don't collide with the title.

### Black deck: pixel icon in the top strip

- A new 12x12 icon `Link` in `ui/icon_map`: two small screens joined by a cable (a nod to the link
  cable). There is also `LinkOff`, the same struck through, like `Gps` / `GpsOff`. The art is the
  builder's within that idea, and it is host-tested like the other icons.
- It is drawn right of the GPS pin in the top strip's empty middle (`ui::topGap`). The pair is
  centred in the gap and uses the strip's icon colours (`theme::pal().topText` on `top`), on the
  battle screen and the status screens, exactly where the pin goes today.
- The black deck's GPS pin stays as it is. During a camera alert or alarm the pin isn't drawn (the
  sign panel and band take over); the link icon follows the same rule, so the strip stays clean in
  an alert. (My call.)
- **Karaoke view** (yellow deck look on the black deck, `ui/karaokeview`): it gets the yellow-style
  text tags `GPS` / `DECK`, right-aligned on its top row (the song and artist row), with the artist
  cut to make room.
  - Check first whether today's camband GPS pin draws over the karaoke view, and through which
    topGap. Whatever does, the karaoke view ends up with the tags and not the pixel pin.
- The black deck's tags and icons show its own view of the link: the yellow deck heard from in the
  last 15 s.

## Constraints

- **Yellow heap and flash:** no new task, no TLS, no new big buffers. `/link`'s reply buffer is
  small (the existing reply buffers fit it). Check the `[mem]` line and the flash % after the build;
  `pio run -e ledpanel` must still fit its slot.
- **Black deck redraw rules:** the top strip is redrawn only whole by the screens, and camband
  calls `invalidate()` after one. Draw the link icon with the GPS pin (same invalidate path), not
  on its own timer that could tear a strip mid-draw.
- **Host tests stay green:** `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1`. The
  new pure code (link state, the `/link` body parse, the new rects, termlayout positions, icon art)
  gets tests.
- **Builds:** `pio run -e ledpanel -e s3` both succeed.
- **Flashing:** the yellow deck is flashed over USB on COM16. The black deck (COM12) is never
  flashed without asking João each time.
- **Repo:** conventional commits; no secrets or private data (config only in `src/config.h`).

## Done looks like

- The changes above, built, with tests passing and both envs compiling.
- The yellow deck flashed and desk-checked by João: the prompt line in the same place on every
  screen, the main screen in layout B, and the tags changing when the black deck is unplugged and
  when GPS 2 IP stops.
- The black deck flashed only if João says yes, then checked the same way.
- `docs/yellow-deck.md` ("On the deck"), `docs/ARCHITECTURE.md` (Between the devices: `/link`;
  Black deck: the icon) and `CHANGELOG.md` updated.

## Left to the builder

- Exact pixel positions, as long as the labels fit (`test_termfont`) and the targets stay big.
- The `/link` body format, and whether the yellow deck parses it with an existing helper.
- The link-cable art.
- How the yellow deck's 5 s check fits the deck task's loop (a timer next to the keep-alives).
