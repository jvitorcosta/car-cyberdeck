# Companion screen: the CYD beside the S3 — design

Date: 2026-10-13. Status: agreed in chat (sections 1-3), awaiting spec review.

## Goal

Turn the old deck (CYD ESP32-2432S028R) into a **companion screen** for the new deck (Freenove
ESP32-S3) in the car: car info, big lyrics and touch buttons on one glanceable dashboard. The
CYD stops talking to Spotify; everything about the song comes from the new deck.

Why: the two decks shared one Spotify app and one rate limit (both blocked ~2 h on
2026-10-13), the CYD's flash is 96 % full and its heap is tight, and the CYD is the board with
a touch screen.

## Decisions (from the brainstorm)

- One dashboard, nothing to navigate while driving: top strip (car info), lyric area, button
  bar.
- **No music controls** (CarPlay does that). Buttons are for the deck: sounds and trip reset.
- Data flow **A**: the CYD polls the S3's `GET /state` (plain HTTP on the hotspot, key
  required) about once a second. Rejected: S3 pushing to the CYD (the S3 would track the CYD;
  a stuck push would block it), ESP-NOW (shares the hotspot's channel, harder to debug, the
  phone page can't use it).
- Weather from **Open-Meteo** (free, no key) for a home location set in `src/config.h`.
- The CYD keeps its current deck build; the companion is a **second build** for the same board.

## Builds

| Env | Board | What it is |
|---|---|---|
| `s3` | S3 | The deck (unchanged role), plus the `/state` additions below |
| `cyd` | CYD | The deck, as today (kept: flash it to go back) |
| `cyd_companion` (new) | CYD | The companion: no Spotify client, Pokemon, album art, walker or lyrics search |

`cyd_companion` uses `build_src_filter` to leave out `spotify/`, `pokemon/`, `images/`,
`lyrics/`, `genre/`, `core/nettask` and the deck's `main.cpp`, and builds `src/companion/`
instead. It shares `ui/` primitives (battle boxes, theme, text, CJK font), `net/wifi` (with
the fixed hotspot address, .11 on the CYD), `net/clock`, `util/` and the board layer.

## New deck (S3) changes

- `GET /state` gains, all optional for older clients:
  - `trackGen` (changes on every song), `progressMs`, `durationMs`, `playing`;
  - `lyric`: `{"kind":"synced|plain|none|instrumental|searching", "line":"...", "next":"...",
    "lineStartMs":n, "nextStartMs":n}`, from the lyrics the deck already fetched (the current
    and next line only, never the whole text);
  - `tripS`: seconds since power-on or the last reset.
- `POST /trip/reset?k=` sets the trip clock to zero (key required).
- `/play` is unchanged (the companion uses it for the sound buttons).

## Companion (CYD) screen, 320x240

- **Top strip (~28 px):** clock (`TIMEZONE`, SNTP), `trip 0:37`, outside temperature with a
  small weather icon drawn in code (sun, cloud, rain, night), WiFi bars, and a dot for the
  link to the new deck (green / grey). Night palette from 18:00 to 06:00 like the deck.
- **Lyric area (~160 px):** synced lyrics: the current line in font 4, wrapped up to 3 rows,
  centred; the next line in font 2, dimmed, below. The CYD switches to the next line on its own
  clock at `nextStartMs` (progress interpolated from the last poll), so changes land on time.
  Plain lyrics follow the timing the S3 already spreads. No lyrics / instrumental: the song
  title and artist big instead. Japanese/Chinese use the existing CJK font; Korean and emoji
  stay blank as on the deck.
- **Button bar (~48 px):** `Howl`, `Vroom` (only when the S3 lists that clip), `Trip`. Trip
  needs a 1 s press. A press flashes the button and shows the answer briefly ("Playing!",
  "Busy"). Buttons are grey while the new deck is unreachable.
- **Touch:** XPT2046 on the SPI host the display doesn't use; raw-to-screen calibration
  constants in `include/pins.h`, measured with a helper in the `hwcheck` build; one action per
  press (no auto-repeat).
- Backlight stays at full (decided 2026-10-07).

## Failure handling

| Situation | Companion behaviour |
|---|---|
| New deck unreachable | Poll every 1 s, every 5 s after 10 misses; grey dot; lyric area "Looking for the deck..."; buttons grey. Clock, trip and weather keep working. |
| WiFi lost | "No WiFi" in the top strip; reconnect via `net/wifi`; no request is started without WiFi (`netcfg::linkUp`). |
| Weather fails | Keep the last value up to 1 h, then `--°`; never affects the lyric area. |
| Wrong key | "Wrong key" in the lyric area (setup problem, visible at once). |
| Song changes | Follow `trackGen`: the old song's lines are dropped as soon as it changes, never shown over the new song. |
| Malformed or partial `/state` | Treated as a missed poll; the screen keeps the last good state. |

## Testing

Host tests (pure modules, one suite each), with **made-up lyric lines, never real song text**:

- `util/stateparse` — `/state` JSON into a struct: full, partial, missing fields, broken JSON.
- `util/lyricclock` — which line to show from `progressMs`, the line starts and the time since
  the last poll; song change resets it.
- `util/pollbackoff`-style rule for the companion's polling (or reuse `netplan::PollBackoff`).
- `util/weather` — Open-Meteo answer parser and WMO weather code → icon.
- `companion/layout` — button rectangles, hit test, 1 s long press.
- Top-strip formatting: clock, trip `h:mm`, temperature.

On the boards: both on the hotspot; lyrics change on time; unplug the S3 → "Looking for the
deck..." and back; buttons; trip reset; `[mem]` on the CYD. `tools/check_flash.py` keeps the
97 % limit for `cyd_companion`.

## Optional extra: retro map (built last)

An original top-down retro-RPG map of where the car is: grass, tree-cluster parks, water,
dirt/paved roads, building blocks and a small car marker, in our own 8x8 tiles and an 8-16
colour palette. No Nintendo/Game Freak art or tiles. Research 2026-10-13 (agent report in
chat) drives these choices:

- **Map data, offline.** A PC tool (`tools/gen_map.py`) turns an OpenStreetMap extract of the
  home region (Geofabrik, filtered with osmium: roads, water, parks, buildings) into a
  **terrain grid**: one 4-bit code per 8x8-pixel cell, zoom ~16 (one cell ~17 m), zlib/RLE,
  ~0.1-1 MB for a city. Stored on the **S3** (spare flash) and git-ignored like the other
  personal data (it is your region). Live sources were ruled out: OSM's tile servers forbid
  bulk/offline use, the public Overpass API is rate-limited and unreliable from a car, PNG
  tiles need PSRAM the CYD lacks.
- **Rendering.** The S3 draws the 40x30-cell window around the car from the grid with our
  tiles, with 4-neighbour autotiling for shorelines and road edges. The companion can show the
  same window: `/state` carries the 600 cell codes plus the car's offset, and the CYD has its
  own copy of the tileset (a few KB).
- **Location.** Neither board has GPS. Recommended: a NEO-6M/M8N GPS module on a free S3 UART
  (TinyGPS++), patch antenna under the windshield. Fallback ruled weak: Safari's Geolocation
  needs HTTPS (a trusted self-signed certificate profile on the iPhone) and only updates while
  the page is in front.
- **Attribution.** "© OpenStreetMap contributors" on the map screen (ODbL).
- **Off the map:** outside the pre-built region, a neutral "unknown area" pattern and the
  coordinates.
- Where it shows: a toggle on the companion (tap the lyric area to swap lyrics ↔ map), and
  optionally on the S3's "Nothing playing" screen.

Needs a decision before planning it: whether to buy a GPS module. Until then, nothing in the
core companion depends on the map.

## Out of scope

Music controls; OBD-II gauges (next project, once the BLE adapter arrives: the top strip and
`/state` leave room for them); the map extra below is optional and built last.
