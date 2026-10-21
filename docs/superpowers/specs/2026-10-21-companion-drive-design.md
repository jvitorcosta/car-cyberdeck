# Companion drive screens

Date: 2026-10-21. Branch `feat/obd`. Follows the gauge cluster (2026-10-19-gauge-cluster-design).

## Why

Once the OBD adapter went live, the gauge cluster took the companion's whole screen for the whole
drive, so the lyrics and her menu (why the companion exists) only showed with the engine off. Her
car-on line and her reactions were said into the serial log, the FUEL % and KM LEFT fields always
read `--` on the City (no PID 2F), and the phone page pressed buttons blind.

## What

- **Drive cluster** (moving): the tach, speed and coolant as before. The FUEL panel becomes TRIP:
  distance and time since power-on (the deck is powered with the ignition, so one drive), km/l now,
  the average; tank % and km left only when the car reports its fuel level. The pedal / throttle
  bars give way to a 48 px **lyric strip**: her small still portrait and the current lyric with the
  next one dim, or her line while she talks.
- **Stopped view**: after 3 s at 0 km/h (5 km/h to move again), her Lyrics screen returns under a
  20 px gauge strip (speed, coolant, volts, trip km), her lines in the message bar.
- **Pinned view** from the phone: AUTO (by speed, the default), DRIVE (always the cluster), LYRICS
  (always the lyrics). Kept in NVS (`companion` / `pin`).
- **Her warnings**: coolant hot and battery low (obdplan's rules) said once when they've held 10 s
  (never the crank dip), again only after a minute clear; the strip inverts while she says it.
  Leaving the car view she tells a dropped adapter link (AdapterLost) from the ignition (CarOff).
- **Phone page**: a status box (car link and speed, the screen and pin, the line on screen) from a
  new `GET /status`, refreshed every 2 s; `POST /view?v=` pins the view.

## Constraints

- Read-only toward the car, unchanged: no new OBD request, nothing outside `obd::allowed()`. The
  trip lives in the companion and restarts at power-on; nothing on the decks resets anything in
  the car.
- The touch layer on the old deck is dead: everything is reachable from the phone.

## Code

Pure and host-tested: `util/driveview.h` (the view), `util/carwarn.h` (when she warns),
`util/carstatus` (the status JSON), the new lines in `util/avatarlines`. Drawing:
`companion/gauges` (TRIP panel, `drawTop`), `companion/carstrip`, `companion/dashboard` (`setTop`);
wiring in `companion/app.cpp`; the page and endpoints in `companion/phone`.
