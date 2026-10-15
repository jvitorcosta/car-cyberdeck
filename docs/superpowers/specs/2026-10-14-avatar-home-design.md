# The avatar as the old deck's home screen

**Date:** 2026-10-14
**Status:** draft for review
**Build:** `cyd_companion` (the old deck). The new deck's firmware doesn't change.
**Builds on:** `docs/superpowers/specs/2026-10-14-avatar-design.md`. That spec's converter, file format, moods, canned lines, typing timeline and singing still apply. Its "beside the lyrics" layout is **replaced** by this spec.

## Revision (2026-10-14): the phone drives her menu

The old deck's touch layer is faulty. It reports one constant press (raw x=0, y≈3977) with this firmware and with `hwcheck`, while the owner tapped with the stylus. The owner chose to drive her from the phone.
- **The page:** the old deck serves its own remote at `http://<old deck>/?k=<REMOTE_TOKEN>` (172.20.10.11 on the hotspot), with LYRICS, SOUNDS, TRIP, BACK, HOWL, VROOM and a hold-to-reset TRIP. The new deck is unchanged.
- **The input path:** `POST /press?b=<name>` queues a button. It goes through the same handling as touch, and LYRICS, SOUNDS and TRIP open their screen from anywhere.
- **Touch stays:** it still works on a good board. A stuck press (the same point for more than 5 s) is ignored.
- **Details:** in the plan's revision, Tasks 7A and 7B.

## Goal

On the old deck, the avatar (Zero Two, from the owner's git-ignored `data/avatar.bin`) is the main interface:
- She's shown big, and everything else is reached through her.
- Version 1 has three functions behind her: **Lyrics**, **Sounds** and **Trip**.
- Weather, Map, OBD and the Claude assistant are later menu items.

## Decisions taken while brainstorming

- **Home screen style:** a Gen-3 battle menu. She's drawn at 2× on the left, and a command box on the right has big touch buttons. Chosen over "tap her, then she asks" and over round icons around her.
- **Default screen:** boot shows Home. Any screen stays until you tap **BACK**. Nothing switches by itself.
- **Her lines:** her own lines show as a speech bubble on whichever screen is up.
- **Input:** the CYD's resistive XPT2046 touchscreen. It's wired on its own SPI bus, and until now only `hwcheck` used it.
- **No avatar file:** the companion keeps today's full-screen lyrics and no menu. The menu is her.

## Screens (320×240, battle style, theme palette, night palette included)

All screens share a **message bar** at the bottom (y 206–239, a dialogue box):
- **When she speaks:** her line types out there (avatarscene: 25 ms per character, then a 3 s hold).
- **Otherwise:** it shows the screen's idle text, or the link status ("Looking for the deck..." / "Wrong key").

### Home

| Element | Where | Notes |
|---|---|---|
| Her portrait, 2× | x 8, y 8, 184×184 | Each 92×92 frame pixel-doubled while it's pushed; same file, no new art |
| Name tag | centred under her, y 194, 9 px | pink, font 1 |
| Command box | x 200–312, y 8–170 | `battleBox` with three buttons, each 100×48: **LYRICS** (y 14), **SOUNDS** (y 66), **TRIP** (y 118); a `▶` cursor on the last one used |
| Message bar idle text | — | "What shall we do, darling?" |

She sings along on Home exactly as the earlier spec describes, and holds the lollipop pose otherwise.

### Lyrics

| Element | Where |
|---|---|
| Her portrait, 1× | x 4, y 4, 92×92, singing along |
| BACK button | x 4, y 104, 92×36 |
| Lyric column | x 104–316, y 0–204: the current line at 2× when it fits in two rows, else at 1×; the dim next line under it (the code built in Task 7R, moved) |

Message bar idle text: empty.

### Sounds

| Element | Where |
|---|---|
| HOWL button | x 20–300, y 16–80 |
| VROOM button | x 20–300, y 90–154, only when the new deck's `/state` lists the clip (`hasVroom`) |
| BACK button | x 20–120, y 164–200 |

- **On a tap:** the companion asks the new deck to play the clip. Her bubble shows "..." while it waits, then her reaction line. If the new deck doesn't answer, she says "I can't reach the deck, darling."
- **Message bar idle text:** "Pick a sound, darling!"

### Trip

| Element | Where |
|---|---|
| Trip time | centred, y 40–110, large font: `h:mm` from `/state`'s `tripS`, ticking locally between polls |
| RESET button | x 20–190, y 130–190; **hold 1 s** to fire; it fills left to right while held |
| BACK button | x 200–300, y 130–190 |

- **Message bar idle text:** "Hold RESET to start a new trip."
- **After a reset:** her reaction line.

## Touch and navigation

- **Driver:** `companion/touch` reads the XPT2046 (HSPI pins in `include/pins.h`). It maps raw readings to screen coordinates (rotation 1) using calibration constants `TOUCH_RAW_X0/X1/Y0/Y1` added to `pins.h`.
  - **Defaults:** 200 / 3900 / 200 / 3900.
  - **Calibration:** set once from `hwcheck`'s raw corner readings.
  - **What counts as a touch:** pressure ≥ 400.
- **Buttons:** a button is chosen where the finger **goes down**.
  - **Normal buttons** fire on **release** if the finger is still inside the button. This is robust against resistive-touch jitter.
  - **Hold buttons** (RESET) fire **once**, after 1 s held inside.
  - Sliding onto another button never fires it.
- **Navigation:**
  - Home → Lyrics, Sounds or Trip.
  - BACK on any screen → Home.
  - Moving to a screen repaints it fully.
- **Night mode:** the night-mode switch repaints the current screen.

## Her lines

- **What carries over:** the boot greeting (first good poll) and song-change remarks (5-min gap, 1-in-3 odds, no `{P}` lines without a Pokémon). They show in the message bar on any screen.
- **New events in `util/avatarlines`.** These always speak; the gap and odds don't apply.

  | Event | Example line | Mood |
  |---|---|---|
  | `Howl` | "Awoo~! The whole street heard that." | happy |
  | `Vroom` | "Vroom vroom~ Faster, darling!" | happy |
  | `TripReset` | "New trip! Where to, darling?" | talk |
  | `Unreachable` | "I can't reach the deck, darling." | smug |

- **Where her face changes:** the mood frames apply to her portrait on Home and on Lyrics. Sounds and Trip don't show her portrait, only the bubble.

## Actions to the new deck

- `companion/link` gains a one-slot action queue:
  - `request(Action)` from the UI loop, where Action is Howl, Vroom or TripReset;
  - `lastResult()` (Pending, Ok or Failed, with the action) for the UI to react to.
- The link task sends it between polls:
  - `POST /play?clip=howl|vroom&k=<REMOTE_TOKEN>`;
  - `POST /trip/reset?k=<REMOTE_TOKEN>`.
  - It uses the same 2 s timeouts as `/state`.
- HTTP 200 is Ok; anything else, or no answer, is Failed. A new request while one is pending replaces it.

## Modules

| Module | Kind | Responsibility |
|---|---|---|
| `util/menunav` | pure, host-tested | Screens, each screen's buttons and rectangles, hit test, navigation (button → next screen), idle message bar text |
| `util/press` | pure, host-tested | Press tracking: button at touch-down, tap on release inside, hold fires once at 1 s, hold progress 0..1 |
| `util/avatarlines` | pure (extend) | The four new events |
| `companion/touch` | device | XPT2046 → screen coordinates |
| `companion/link` | device (extend) | Action queue and POSTs |
| `companion/avatarview` | device (rework) | Portrait at 1× or 2× at a given spot, name tag, events, her line for the message bar |
| `companion/screens` | device | Draws Home, Sounds and Trip, the message bar and the buttons (pressed, held fill) |
| `companion/dashboard` | device (rework) | The Lyrics screen: corner portrait, BACK, lyric column |
| `companion/app` | device | Screen state, touch → press → action / navigation, per-frame update |

## Failure handling

- **No avatar file:** today's full-screen lyrics, with touch ignored.
- **New deck unreachable:** Home and the screens still work. Sounds and Trip actions fail with her "can't reach" line. The message bar shows "Looking for the deck..." when she's quiet.
- **Touch not calibrated:** the buttons are big enough that the default calibration should work. `hwcheck` gives the values to fix it.
- **A touch while she types:** it works normally. Her line keeps typing in the bar.

## Testing

**Host tests:**
- `menunav`: rectangles inside the screen and not overlapping, hit test at edges, every BACK goes to Home, VROOM only when listed;
- `press`: tap fires on release inside, not after sliding out; hold fires once at 1000 ms, not on a 999 ms tap, never twice in one hold; sliding onto another button doesn't fire it;
- `avatarlines`: the new events always speak, with the right moods.

**On the old deck:**
1. Calibrate with `hwcheck` (corners).
2. Tap every button.
3. Hold RESET; check a short tap does nothing.
4. HOWL plays on the new deck.
5. Unplug the new deck: the "can't reach" line shows.
6. Lyrics: she sings in the corner, and BACK works.
7. Night mode repaints.
8. `[mem]` is steady over 10 min.

## Out of scope (later)

- Weather, Map, OBD and the Claude assistant as menu items (the command box has room for a fourth).
- Blinking.
- Auto-return to Home.
- Swipes.
