# Yellow deck: only 2 of 8 car lights on (2026-11-14)

What João saw in the car: of the 8 lights (4 door lights, 4 footwell lights under the seats),
only the footwells under the driver's seat and the front passenger's seat light up. The two rear
footwells and the door lights stay dark.

Short answer: **no change in #50–#58 can light some strips of a controller and not others.** The
deck has no pins, channels or LED counts for the lights. It sends one colour to each of two
Bluetooth controllers, and each controller drives all of its own strips. So "front footwells on,
rear footwells off" comes from the controller or the wiring after it, unless the controller has
been left running one of its own effects (cause 2 below). The dark door lights are more likely to
have a deck-side reason (link, saved off, or very low level), so check those first.

## What the code drives today

There are no GPIO outputs, PWM channels or LED counts for the car lights anywhere in the firmware.
The yellow deck (`env:ledpanel`) is a Bluetooth LE client of two controllers:

| Light | Controller | Address from | Index | What the deck can set |
|---|---|---|---|---|
| INSIDE | MELK-OA25PQ (Lotus Lantern app) | `LED_INSIDE_MAC` in `src/config.h` | 0 | power, one RGB colour, its built-in effects (mode + speed) |
| OUTSIDE | ELK-BLEDDM (Magic Carlight app) | `LED_OUTSIDE_MAC` | 1 | power, one RGB colour (effect frames are ignored) |

- Controller table: `src/yellow/ledpanel.cpp:235-238`. One link each, service FFF0,
  characteristic FFF3, write without response: `src/yellow/elklink.cpp:135` (connect),
  `src/yellow/elklink.cpp:170` (write).
- Frames: `src/util/yellow/elkcmd.h`. A colour is 9 bytes `7E 07 05 03 RR GG BB 10 EF` (plus the
  `7E 00 …` variant). It carries no zone, segment or pixel number.
- Every look goes out as power + colour in both variants: `pushNow`,
  `src/yellow/ledpanel.cpp:364`.
- Brightness is done by scaling the colour itself: `elkcmd::dim`,
  `src/util/yellow/elkcmd.h:34` (perceptual, pct²). The lowest level is
  `ledlook::MIN_PCT = 10` (`src/util/lights/ledlook.h:67`).
- The IR receiver added in #58 is on GPIO 27 (`include/pins.h:39`). It only reads the remote. It
  has nothing to do with the lights' power or wiring.

What this means: the deck cannot address "rear footwell" or "door 3". Whatever it sends, all strips
on one controller get the same frame. Which of the 8 lights hangs off which controller isn't written
down anywhere in the repo (the code only knows INSIDE and OUTSIDE). **Step 0 tonight: note which
controller feeds the footwells and which feeds the doors.**

## What #50–#58 changed around the lights

| Commit | What changed for the lights | Can it explain the symptom? |
|---|---|---|
| e7ed4f7 (#50) | BLE moved from NimBLE to Bluedroid's GATT client (new `src/yellow/elklink.cpp`), shared with the PS3 pad's Bluetooth Classic | It can drop a **whole controller's** link (all its lights out), never part of a controller |
| 9c15fcc (#51), 4728263 (#53), b98c0c2 (#54), 8b5c9bc (#55) | pad, menu, karaoke | No light frames changed |
| 3705276 (#52) | removed weather, cams, car health, OTA (−1167 lines in `ledpanel.cpp`) | Light code untouched (frames, link, looks the same) |
| fc9bec5 (#57) | PS3 shortcuts for lights; `bothlook` keeps each light's own level | Lights can be left at different levels and off/on states |
| bcd0b63 (#58) | IR remote: ☀▼ steps **both** lights down 10 % at a time, ~4/s while held, saved to NVS (`ledpanel.cpp:1786-1791`); OFF turns both off; JUMP/FADE send effect frames with placeholder modes `{0,1,2,3}` (`src/util/yellow/irkeys.h:58`, `irEffect` at `ledpanel.cpp:1707`) | Yes, for "dark" lights: see causes 2 and 3 |
| c13573f (#47, just before) | Welcome: the inside controller runs its own curtain effect `FILL_MODE = 58` (`src/util/lights/ledintro.h:17`), "fills from both ends inwards", then a colour frame should end it. **Never seen in the car** | Yes, for "only the front footwells": see cause 2 |

No pin, LED count, strip length, power-limit or init code was changed or removed in this range.
I found no clear code bug.

## Likely causes, ranked

### For the rear footwells (front two on, rear two off)

1. **Hardware after the controller (most likely, ~65 %).** One controller sends one signal to all
   its footwell strips. Two strips on and two off under the same signal means the signal or power
   isn't reaching the rear two: the splitter or extension lead to the rear seats, a connector
   kicked or pulled loose under a front seat (seat rails, mats, the PS3 and IR wiring work under
   the dash), a pinched wire, or a dead strip. Front strips are usually on the short leads and rear
   ones on the long extensions, so a front-on, rear-off pattern is the classic wiring failure.
2. **The inside controller stuck in one of its own effects (~25 %, only if the footwells are the
   INSIDE light).** Two ways the deck leaves the MELK running an effect:
   - the welcome's curtain, `FILL_MODE 58` (`ledpanel.cpp:349`, `writeKey` at `:325-342`), new
     since #47 and never checked in the car. If the footwells are one addressable chain that runs
     driver → rear → rear → passenger, a curtain that "fills from both ends inwards" and stops
     early (or isn't ended by the colour frame on this OA25PQ, which the code assumes from notes on
     a different model, the OA10) lights **exactly the two front ends**;
   - a JUMP/FADE press on the IR remote: modes 0–3 are guesses (`irkeys.h:58`), and some MELK
     modes only light part of the strip.
   Either one clears with a colour press. If a colour key brings back all four, it's this.
3. **Very low level (~10 %).** At 10 % a full channel is sent as 3/255 (about 1 % duty,
   `elkcmd::dim`). Strips with longer leads (voltage drop) or another batch can look fully off
   while the front ones glow. The IR ☀▼ makes this easy to hit: 2–3 s held goes from 100 % to
   10 %, and it's saved, so a reboot keeps it.

### For the door lights (all off)

If the doors are a whole controller of their own, "all four off" points at one thing for that
controller:

1. **No link to it** (#50's Bluedroid link): the screen says `>OUTSIDE: NO LINK` (or INSIDE), pad
   LED 3 (outside) or 2 (inside) is off, `/status` says `nolink`.
2. **Saved off or near 0 %**: an IR OFF, a PS3 shortcut, or ☀▼ held. `/state` shows it.
3. **Hardware for that kit**: no 12 V (fuse, add-a-fuse tap, switched feed), or the controller
   itself. The outside kit also has its own IR receiver outside the car; the kit remote can still
   switch it off directly if that receiver catches it.

**How likely is it hardware overall?** For the rear footwells, likely, because no firmware path
can split one controller's strips. Only an effect left running on the controller (cause 2) is an
exception, and a single colour press rules it out. For the doors, check the link and the saved
level first. Those are cheap to check and #50/#58 touched them. If the deck shows the outside light
linked at a normal level and the doors stay dark, it's the kit's power or wiring.

The quickest test that separates them, before any bench work: **in the car, with the deck
unplugged** (the controllers only advertise while nothing is connected), turn the iPhone's
Bluetooth on and set white at 100 % from the controllers' own apps (Lotus Lantern for the MELK,
Magic Carlight for the ELK). If the rear footwells or the doors stay dark from the official app,
it's hardware, and the firmware is cleared.

## Bench plan for tonight (deck on the PC)

The controllers stay in the car, so the bench checks what the deck **sends and remembers**. The
lights themselves can only be checked in the car (or with a controller and a 12 V supply on the
desk).

1. **Firmware on the deck.** Open the serial port (115200, the CH340 port, COM17 behind the
   extender; DTR/RTS low so it doesn't reset). Find `[boot] ledpanel v…+g<sha>` and check the sha
   is `c7110b2` (current main) or what you last flashed.
2. **The saved looks.** On the hotspot, `GET /status?k=…` and `GET /state?k=…&zone=inside`, then
   `&zone=outside`. Look for `off` or a level of 10–20 %. If you find one, that's cause 3 or the
   doors' cause 2. Fix it with ☀▲ or ON and see if it changes the car next time.
3. **The links at boot.** With the controllers out of range you'll see repeated
   `[leds] INSIDE: connect failed (…)` and `[leds] OUTSIDE: connect failed (…)` every 5 s → 60 s.
   That's expected on the desk. Check that **both** are tried, that there's no
   `[leds] …: no address in config.h` and no `GATT client: …` error, and that `[mem] … min` stays
   above ~25 KB.
4. **The welcome's frames.** `python tools/intro_check.py COM17` → expect PASS: each key logged
   (`[intro] INSIDE +300 ms key 2/4: fill speed 50 effect 58, no link`, then the colour keys at
   +4300 and +4600 ms). That shows the deck **would** end the curtain with a colour frame.
   Whether the OA25PQ obeys it only shows in the car.
5. **IR on the desk.** Press ☀▼ a few times and watch `[ir] key …` and the level drop. Press a
   colour and check `[ir]` and the screen. JUMP/FADE should log `[ir] effect mode 0..3`.

## In the car next (5 minutes, in this order)

1. Deck on. Look at the prompt line and the pad LEDs: both lights `LINKED`?
2. Press **ON**, then **WHITE**, then **☀▲** until 100 % on the IR remote. All four footwells now?
   - Yes → it was the level (cause 3) or an effect left running (cause 2). Then reboot the deck and
     watch the welcome: if the rear footwells stay dark after the fill, `FILL_MODE 58` is the
     culprit (see the fix below).
   - No → go on.
3. Unplug the deck, phone Bluetooth on, white 100 % from Lotus Lantern / Magic Carlight.
   Still dark → hardware, the firmware is cleared. Phone Bluetooth off again afterwards
   (Settings, not Control Center), or the deck can't connect.
4. Hardware, with a multimeter, controller first and then along the leads:
   - 12 V at each controller's input with the car on (and with it off, if the kit is on a switched
     feed);
   - for an RGB strip (4 pins): +12 V on the common pin at the rear strip's connector, and the
     R/G/B pins pulled low when that colour is set. A missing +12 V is the lead or splitter, and
     present voltages with a dark strip point at the strip;
   - for an addressable strip (3 pins: V+, data, GND): V+ and GND at the rear connector. If the
     power is there, the break is the data line between the front and rear strips (the last lit
     LED marks where it breaks);
   - wiggle the connectors under both front seats and at the splitter while the lights are on.
   - In Lotus Lantern, check the MELK's settings (LED count / IC type / RGB order). If the LED
     count is lower than the real strip, only the first strips light up. The deck never sends this
     setting, but the app or a reset can change it.

## If it turns out to be the welcome (cause 2): the fix, not applied

The code assumes the OA25PQ ends an effect when a colour frame comes (`ledintro.h` comment,
`elkcmd.h:24`). If the car shows otherwise:

- Quick and safe: in `src/util/lights/ledintro.cpp`, `plan()` for the inside light, put a
  **power-off/on pair** (or a "static colour" effect, once the probe build finds its number)
  before the last two look keys, so the controller leaves the effect whatever it does with colour
  frames. The same goes at the end of `irEffect` when the next colour key comes.
- Or find the right curtain first: flash `ledpanel_probe` (JUMP3/JUMP7 step the mode, FADE3 sends
  it), try 57–76 in the car and set `FILL_MODE` to the one that fills the whole run.
- Or turn the welcome off for the inside light until it's tuned (`startIntro` only for `i == 1`,
  `ledpanel.cpp:3324-3328`).

None of this is applied in this branch. This PR only adds this file.
