# Camera alerts: speed cameras, electronic speed bumps, red-light cameras

Status: draft for the owner's review (2026-11-03). Runs on the black deck (the ESP32-S3), because the
OBD deck from `feat/car-health` doesn't exist as hardware yet.

## Intent

Warn the driver before every fixed enforcement point in Manaus: speed cameras ("controlador de
velocidade fixo"), electronic speed bumps ("lombada eletrônica") and red-light cameras ("avanço de
sinal vermelho"). An alert says what is ahead and its limit; an alarm fires when the car is over
that limit close to the camera. It is the same idea as Waze's camera alerts, but offline and on the
decks.

Inspiration: a reel by @andreazevedo.dev (2026-11-01) that loads São Paulo's CET camera list into
an ESP32 and alerts by GPS. Manaus has no open list like CET's, so most of the work here is the data.

**Legal line:** this is a GPS position check against a list of known places. Nothing detects or
receives radar signals: a radar detector is banned in Brazil (CTB art. 230, III), and that line
must never be crossed, not even as an experiment.

## Goals

- **Three kinds of point**, each with its own look and sound: speed camera, speed bump, red-light
  camera.
- **Alert** on approach: the kind, the limit (none for red-light), a distance countdown and the
  current speed.
- **Alarm** when the car is over the limit inside the alarm zone. It stops by itself once the speed
  drops below the limit or the point is passed.
- **Direction-aware:** only points enforced in the car's direction and ahead of it, so a camera
  facing the opposite lane, or one on a parallel road, stays silent.
- **Works offline:** the whole list lives on the deck; no network needed while driving.
- **Easy to keep current:** a PC tool rebuilds the list from its sources, and the driver can mark a
  new camera from the car.

## Non-goals (this version)

- Anything that senses radar, laser or any other signal (illegal, see above).
- Mobile radars, police checks, crowd reports (Waze's live data is closed).
- Average-speed (section) enforcement: not used in Manaus.
- Cities other than Manaus; the format allows them, but only Manaus gets curated data.
- Downloading the list on the deck while driving.
- OBD speed: there is no OBD deck yet (see "Later").

## Anti-goals (failure even if it works)

- **False alarms:** an alarm for the opposite lane, for a parallel street, or for an overpass makes
  the driver ignore it within a week.
- **An alarm that won't stop:** it must stop as soon as the car is under the limit or past the point.
- **Distraction:** alerts and alarms are the one thing allowed to beep while moving, so everything
  else stays quiet. No animation beyond the countdown, and no more than one alert on screen at a
  time.
- **Silent failure:** if the GPS has no fix, the decks must say so, never pretend the road is clear.
- **Hurting what works:** the black deck's Spotify, lyrics and screens must keep running as before
  (its heap is watched by the self-heal check; the GPS work must stay small).

## Data

### Sources (checked 2026-11-03)

| Source | What it gives | Gaps |
|---|---|---|
| IMMU "Localização dos Radares" page (`immu.manaus.am.gov.br/index.php?r=site%2Finforadar`) | 13 fixed speed cameras, 2 speed bumps, 5 red-light cameras, each as an address with a direction ("sentido Sul") | no coordinates, no limits |
| IMMU "Levantamentos Técnicos" PDFs (same page, 7 on Av. do Turismo) | the limit (e.g. 60 km/h), the direction, lanes, and an aerial photo with the camera marked | no coordinates; only Av. do Turismo |
| OpenStreetMap (`highway=speed_camera` in the Manaus box) | 4 nodes with coordinates and `maxspeed` (60, 60, 60, 80) | no direction, very incomplete |
| News (2025-2026) | the city added 17 cameras in July 2026 and 34 since February | names streets only; the IMMU page may lag behind |
| The driver, from the car | exact position and heading of a camera they pass | only roads they drive |

No single source is complete, so the list is merged and curated on the PC.

### The list

One file, `data/cameras.csv` (UTF-8, one row per point, sorted by id), committed to the repo:
the locations are public (IMMU publishes them).

| Field | Meaning |
|---|---|
| `id` | stable short id (`mao-0001`) |
| `kind` | `speed`, `bump` or `redlight` |
| `lat`, `lon` | WGS84, 6 decimals |
| `heading` | degrees (0 = north) of the enforced direction of travel; required (`push` refuses a row without one) |
| `limit` | km/h; empty for `redlight`; empty for another kind only while unknown (the tool lists those rows) |
| `label` | short place name for the screen ("Av. do Turismo · Vida Film") |
| `source` | `immu`, `immu-survey`, `osm`, `car` |
| `checked` | date the row was last confirmed (`YYYY-MM-DD`) |

About 50 rows fill only a few KB. On the black deck the list lives in the media partition, next to
the intro clips, and is loaded into RAM at boot (a few KB).

### The PC tool (`tools/cameras.py`)

- `fetch`: reads the IMMU page and the OSM query, and reports what is new or gone compared with
  `cameras.csv` (by label and distance), without editing the file.
- **Placing a point:** an IMMU row has only an address. The tool geocodes it (Nominatim) to a
  first guess and writes a small HTML map (Leaflet, OSM tiles) where the owner drags the pin onto
  the road and sets the arrow for the direction. The tool then writes the row. Every point, from any
  source (IMMU, OSM, the car), gets placed by hand once and gets its arrow; a geocoded guess or an
  OSM node never goes to the deck unconfirmed. Placing also asks for the limit, prefilled from the
  survey or OSM's `maxspeed`; the owner confirms it from the road sign or leaves it empty.
- `import-car`: pulls the points marked in the car (below) off the black deck and places them the
  same way.
- `push`: uploads `cameras.csv` to the black deck over WiFi (a signed request, like the other
  deck-to-deck requests); the deck reloads it without a reboot.
- `check`: run monthly by hand or by a routine; it runs `fetch` and says whether anything changed.

## Hardware

- **GPS from the owner's iPhone** (2026-11-03, the owner's choice: no module to buy for now). The
  iPhone app GPS 2 IP (App Store id408625926, runs in the background) serves the phone's GPS as
  NMEA over a TCP socket. The decks already join the iPhone's hotspot, so the black deck connects as
  a TCP client to the phone (the hotspot's gateway, `172.20.10.1`, port from `config.h`, the app's
  default 11123) and reads `$GPRMC` sentences (about 1 Hz). Reconnects with a back-off; a dropped
  socket shows as `GPS --` after 5 s like a lost fix (reviews say the app's link drops now and then
  without warning, so this path matters). A u-blox module on a UART can replace it later behind the
  same NMEA reader.
- **Speed:** GPS speed (the only source until an OBD deck exists).
- **Sound:** the black deck's own speaker (ES8311 on its single I2S port). Alerts take priority:
  they interrupt the intro or the waiting-screen music box, then let it continue.

## How it decides (black deck, `src/util/car/cameras.*`, pure and host-tested)

The matching code has no board dependencies, so it can move to the OBD deck later unchanged.

Every GPS fix (about 1 Hz from the phone):

1. **No fix or moving under 10 km/h:** no alerts. If the fix is lost for more than 5 s while moving,
   the state is `nofix`.
2. **Candidates:** points within 800 m (a precomputed grid of about 1 km cells keeps this cheap).
3. **Same direction:** the car's course within ±40° of the point's `heading`.
4. **Ahead and on this road:** the bearing from the car to the point within ±30° of the course,
   and, inside 200 m, the point's sideways (cross-track) distance from the car's line of travel
   under 30 m. This keeps out service roads beside an avenue and overpasses running the same way
   (our call: no road geometry per point; if a real place still false-alarms, the fix is a
   per-row override, not a new format). Once the car has been within 60 m of a point and the
   distance starts to grow, that point counts as passed and is ignored for 60 s.
5. **The nearest remaining point** becomes the active one.

The stages for the active point:

| Stage | Speed camera / bump | Red-light |
|---|---|---|
| **Alert** | at 400 m, or 20 s ahead at the current speed, whichever is further | at 250 m |
| **Alarm zone** | the last 200 m | none (alert only) |
| **Alarm** | in the alarm zone with speed > limit | — |
| **Clear** | passed | passed |

The limit check uses the displayed speed rounded down, with no tolerance (our call; one constant,
easy to change). A point with an unknown limit alerts with `--` and never alarms.

The black deck plays the sounds itself, straight from the stage changes, so a lost WiFi link
between the decks never silences an alarm.

## Screens and sound

### Black deck (the one that knows)

- **Alert / alarm:** a band across the bottom of the battle screen (or whatever screen shows):
  the kind's icon, `RADAR 60`, `LOMBADA 40` or `SINAL`, the distance and the speed. Red for an alarm.
  This is the fallback the driver can always trust, even with the yellow deck off.
- **No fix:** a small `GPS --` tag in a corner; no band.

### Yellow deck (the dashboard)

- The black deck serves `GET /cam` (signed, like `/state`): `{"st":"alert|alarm|nofix|off",
  "kind":"speed","limit":60,"dist":320,"kmh":58,"label":"…","id":"mao-0007"}`. The yellow deck
  polls it every second at all times (a tiny reply; the existing `/state` poll stays as it is). The
  screen lags at most about 1 s (17 m at 60 km/h), which is fine against 400 m of warning.
- **Alert:** a band across the bottom third of whatever screen is showing (dashboard, LYRICS,
  PRESETS). It shows the kind's icon, `RADAR 60`, `LOMBADA 40` or `SINAL`, a big distance countdown
  ("320 m") and the current speed. It uses the CYBERDECK green palette; orange for red-light.
- **Alarm:** the same band in red, with the speed shown large and blinking at 2 Hz. It is the only
  blinking element on the deck.
- **No fix:** a small `GPS --` tag on the dashboard's status row; no band. If `/cam` stops answering,
  `CAM --` instead (the black deck still beeps).
- The band disappears 2 s after the point is passed.

### Sound (black deck speaker)

- An alert plays one short double beep per kind (speed: two mid tones; bump: two low; red-light:
  one long high). The alarm repeats a fast triple beep every 1.5 s until it clears.
- A "mute alerts" toggle keeps the bands but silences the sound; it is reached by voice
  ("cameras mute", through the yellow deck's `/say`), the watch and the yellow deck's touch, and
  lives on the black deck. It resets at each start.

## Marking a camera from the car

- Voice "mark camera" (Siri → yellow deck → black deck) or a MARK item on the watch saves the
  current position, course and time to `marks.csv` on the black deck. Siri replies "marked". No
  touch target (our call: a new camera has no band to press, and the screens' touch areas are taken).
- Marks never alert by themselves: they wait for `tools/cameras.py import-car` on the PC, where the
  owner sets the kind and limit and places the pin. This way a mistaken mark can't cause false
  alarms.

## Phases

1. **GPS on the black deck:** the phone's NMEA socket read on its own small task, position, speed and
   course in `/cam`; `GPS --` on both decks. Bench-testable with a recorded NMEA file fed through
   a small TCP server on the PC in place of the phone.
2. **The list:** `tools/cameras.py` with fetch, placing and push; the first curated Manaus list
   (the IMMU rows, the Av. do Turismo surveys, the OSM nodes).
3. **Alerts and alarms:** matching in `src/util/car/cameras.*` with host tests (approach, opposite
   lane, parallel road, overpass, passed, unknown limit, no fix), the bands on both decks and the
   sounds on the black deck. A 30-min drive with Spotify playing must keep the self-heal numbers
   where they are.
4. **Marking from the car** and `import-car`.

## Later

- When the OBD deck exists: the alarm compares OBD speed (what the dash shows; GPS lags in tunnels
  and on bridges). Whether the GPS and the matching move to the OBD deck is decided then; the pure
  code moves as is.

## Left to the builder

- The NMEA parser (TinyGPS++ or a small RMC/GGA parser of our own).
- How the beeps share the I2S port with the intro and the music box (generated tones are enough).
- The grid size and the exact geometry maths (an equirectangular approximation is enough inside a
  city).
- The icons (in the existing 1-bit style) and the exact tones.
- Host-test fixtures: recorded or synthesised drives past real Manaus points.
