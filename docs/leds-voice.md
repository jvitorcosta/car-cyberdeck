# Car lights by voice (Siri Shortcuts)

The LED panel on the old deck (`pio run -e ledpanel -t upload`) takes spoken commands from an
iPhone Shortcut over the hotspot. Siri turns your voice into text, the Shortcut sends it to the
deck, the deck changes both lights and answers with one line that Siri speaks back.

- Address: on the iPhone hotspot the deck is always `172.20.10.11`.
- `GET /say?text=<phrase>&k=<REMOTE_TOKEN>` changes the lights; the reply is e.g. `Purple, 70 percent`.
- `GET /state?k=<REMOTE_TOKEN>` answers the same line without changing anything.
- Both take an optional `&zone=inside|outside`: then only that light's page changes (or is read),
  and the reply is prefixed: `Inside: Purple, 70 percent`. Without it, both lights (the BOTH page).
- `GET /presets?k=<REMOTE_TOKEN>` lists the four presets, one line each, joined by newlines:
  `1 ROSE pink 40 pink 100` (slot, name, inside colour and level, outside colour and level). The
  list is fixed, so it never changes between calls.
- `GET /status?k=<REMOTE_TOKEN>` tells whether each light is linked and on:
  `inside=on outside=nolink`. Each word is `on`, `off` or `nolink`; a light the deck has no link
  to is always `nolink`, whatever it last showed.
- All of them reply in plain text (`text/plain`). A wrong key gets 403 "Wrong key" (on all of them); a phrase with
  nothing to do gets "Sorry, I didn't get a colour or a command".
- Siri sends no zone, so voice always changes both lights; spoken zone words are ignored. The page
  on the deck's screen is left as it is.

## On the deck

The deck boots by typing the welcome (`WELCOME TO CYBERDECK`, character by character in about half
a second), then shows the **main screen**, whatever page it last showed. The look is "Seegson"
(Alien: Isolation): pale green on black, one hue at three brightnesses, and red kept for problems
only (`-- NO LINK`, `>OFFLINE`, the `NO LINK` messages, `>WRONG KEY`; a deeper red at night). Every
screen has faint scanlines and ALL CAPS bracket labels. To read at a glance in a moving car, every
colour clears a contrast on black of at least 4.5:1 by day (the dimmest green too) and 3:1 at night
(WCAG; `src/util/termpal.h` lists each one's figure), and the darker greens carry no blue, which
the panel would turn teal.

- **Main screen** (a dashboard, top to bottom; it puts first what the car's panel and CarPlay don't
  show: the trip and the lights):
  - the header `CYBERDECK // HONDA CITY TOURING 2022`;
  - the clock big with the date under it (`FRI 23 OCT`; `--:--` and `--- -- ---` until WiFi has
    synced the clock), and on the right the outside temperature big (`32C`, `--C` before the first
    reading) and the sky in words (`PARTLY CLOUDY`); under them, on the right, `HI 32 LO 26  MANAUS`
    (dim; `HI 32 LO 26` if a city were ever too long for the row). Where the car is comes from
    the hotspot's public IP (ip-api.com, approximate), the weather from Open-Meteo, every 15 min.
    A failed fetch keeps the last reading; `>OFFLINE` shows only when no reading ever came;
  - the trip: `TRIP`, its time big (`0:32`), and on the right the distance (`12.4 KM`, or
    `-- NO LINK` while no speed source feeds it);
  - both lights on one row, split by a thin rule: `IN`, a chip of its colour, the colour name and
    level (`ROSE 40%`) on the left, `OUT` the same on the right; `OFF` (dim) for a light that is off,
    `-- NO LINK` (red) when the deck has no link to it (never its last colour);
  - the prompt line: `>` and a blinking block cursor; a message while one shows (see Messages), and
    music mode's readout while it is on;
  - at the bottom, two big buttons (44 px tall): `> [LIGHTS]` opens the Lights page, `> [KARAOKE]`
    the black deck's lyrics. KARAOKE is bright with a small inverted `LIVE` tag at its right edge
    while the black deck plays a song with synced lyrics, and dim (no tag) otherwise: a tap then
    says why. The song itself is on the Karaoke page only.
- There is no Weather page any more: its data is on the main screen. The presets are on the Lights
  page only (and voice, the watch, `/presets`).
- **Lights**: `< BACK` (the same boxed button as on Karaoke) returns to the main screen. Then
  Pip-Boy style tabs for BOTH / INSIDE / OUTSIDE, the status line, the 2x4 colour boxes (a colour
  chip and the name; the chosen one is the inverted box), the presets (the name over two colour
  chips, inside and outside), and a 10-cell level bar with `-` / `+` (its label is the %, `MIX` or
  `OFF`). Colour and preset names share one size. Tap a tile to set the colour and turn on;
  double-tap the lit one to turn off. The Karaoke page's song row is in capitals like the rest of
  the chrome; the lyrics keep their case.
- **BOTH while the lights differ** (after a preset like ROSE, whose lights differ, or after the
  INSIDE / OUTSIDE pages): no tile is ringed and the level box says `MIX`. On/off (Y,
  voice "off", the watch's Off) and `-` / `+` (LB / RB, "brighter" / "dimmer") act on each light's
  own look, so each keeps its colour and level; a spoken level ("50%") sets both to that level and
  keeps each colour. A colour (a tile, or spoken) sets both lights to it, as usual.
- **Status line**: just the link state. `LINK 2/2` while the deck holds both lights; when one is
  down, in red, `OUTSIDE: NO LINK` or `INSIDE: NO LINK`, or `NO LINK` for both. Each light's colour
  and level are on the main screen (on Lights: the tiles and the level bar of its page). What the
  deck shows as on is what it last sent while connected, not a reading from the controller (the
  LED controllers can't be read back).
- **Messages**: a terminal line shows on the main screen's prompt line (on
  Lights: in place of the status line) for 2.5 s when something happens: `>ROSE APPLIED` (a preset,
  from any source), `>INSIDE: NO LINK`, `>OUTSIDE: LINKED` (a light's link changed), why KARAOKE
  won't open (`>LAST TRIP ...` at boot, 6 s); the blinking cursor moves to its end. Then the prompt
  line is back to `>` (on Lights: the status line).
- **Xbox controller** (BLE): D-pad moves the focus (an inverted bar on the main screen's two
  buttons, left / right between them; a green ring on Lights), **A** presses, **B** is Back (to the
  main screen; there it does nothing). On Lights only: **X** next page, **Y** this page's lights
  on/off, **LB / RB** `-` / `+`. B used to switch to the BOTH page; use X for that now. The focus
  shows only while the controller is connected: without it the main screen's two buttons look
  alike, so the clock and the lights lead.

## Presets

Four fixed presets replace the old saved favourites. They are in the firmware, so nothing is saved
on the deck and there is no hold-to-save. Each sets **both** lights, both on:

| # | Name | Inside | Outside |
|---|---|---|---|
| 1 | ROSE | pink 40 % | pink 100 % |
| 2 | FIRE | red 50 % | red 100 % |
| 3 | CALM | white 40 % | white 40 % |
| 4 | PURPURA | purple 40 % | purple 100 % |

- **Apply**: tap a preset on the Lights page, or Xbox A on it. That is all: no hold.
- **Voice**: "rose", "fire", "calm" or "purpura", or "preset one" ... "preset four" (also
  `favourite`/`favorite`, and digits: "preset 2"). It applies to both lights, whatever zone was
  asked, and ignores the rest of the phrase ("rose 50" is just ROSE). The reply is e.g.
  `Rose: inside Pink 40 percent, outside Pink 100 percent`. Siri may hear "purpura" as "purple",
  which is the colour; say "preset four" for that one.
- **Watch**: the Honda City app has a fourth page, **PRESETS** (the names with the two dots each).
  START there lists them by name; pick one to apply it. "No presets yet" shows if `/presets` didn't
  answer.
- Presets with different looks inside and outside make the BOTH page show `MIX` (see "BOTH while
  the lights differ" above); the main screen shows each light's colour and level.

## Night mode

From **18:00 to 06:00** local time, once the deck has the time over WiFi, the screen goes to an
amber palette (dimmer than the day green, with nothing left green) and the backlight to about a third. It changes only the screen, never the
lights. Without WiFi (no `REMOTE_TOKEN`) there is no clock, so the deck stays in day mode.

To check it without waiting for evening, build with the test hook, then clear it:

```
$env:PLATFORMIO_BUILD_FLAGS="-D NIGHT_TEST"; pio run -e ledpanel -t upload
Remove-Item Env:PLATFORMIO_BUILD_FLAGS
```

## Theme: the inside light's colour

While the **INSIDE** light is on and linked, the whole terminal (main screen, Lights, Karaoke; not the
boot typing or the touch calibration) takes that light's colour: the same three brightnesses in its
hue, by day and darker at night (`termpal::rgb(role, night, theme)`, every colour's contrast in
`src/util/termpal.h`). Blue and purple are lighter shades so the text reads; green by day is the usual
green; white is a pale grey-white by day and the usual **amber at night** (no white glare). Problems
stay red, except on the red, orange and pink themes, where they show in **white** (a dimmer white at
night) so they stand apart from the text. The colour tiles, chips and preset chips keep their real
colours. With the inside light off or out of reach the screen goes back to the green / amber. To
see a theme without the car's lights, build with `-D THEME_TEST=<0..7>` (red, orange, pink, purple,
blue, green, white, yellow) the same way as `NIGHT_TEST` above, then clear it.

## What it understands

English or Portuguese, any order, other words ignored (`src/util/ledvoice.h`):

| Kind | Words |
|---|---|
| Colours | red/vermelho, orange/laranja, pink/rosa, purple/roxo/lilás, blue/azul, green/verde, white/branco, yellow/amarelo (also Siri's "read" = red, "blew" = blue) |
| Power | on, off / liga, desliga, acende, apaga |
| Brighter / dimmer (10 % steps) | brighter, up, more / mais, aumenta, forte; dimmer, down, less / menos, diminui, fraco |
| Level | a number (`50`, `50%`, rounded to 10), max/máximo, min/mínimo |
| Preset | rose, fire, calm, purpura; or preset/favourite/favorite + 1-4, as a word or digit ("preset two", "preset 2") |

A colour also turns the lights on.

## Shortcut 1: "Lights" (say anything)

In the Shortcuts app, new shortcut:

1. **Dictate Text**: language English (or Portuguese), stop listening *After Pause*.
2. **URL Encode** the Dictated Text.
3. **Text**: `http://172.20.10.11/say?k=<REMOTE_TOKEN>&text=` followed by the URL Encoded Text
   variable.
4. **Get Contents of URL** of that Text (method GET).
5. **Speak Text**: Contents of URL (or *Show Result*, for a silent version).
6. Name it **Lights**.

"Hey Siri, lights" → Siri listens → "purple" → the lights turn purple and Siri says "Purple, 70
percent". The first run asks to allow connecting to 172.20.10.11: allow.

## Shortcut 2: one phrase, no question

For the commands used most, one shortcut each skips the "what?" step. Same as above without
steps 1-2, with the phrase in the URL, e.g. `...&text=purple`, named **Purple lights**: "Hey Siri,
purple lights". Likewise "Lights off", "Lights dimmer".

## Which language, and the name

Tested on 2026-10-21 with the shortcut named **Cyberpunk** ("Lights" was taken by HomeKit, and
English Siri misheard "Luzes"):

- **"Hey Siri, run Cyberpunk"**: Siri itself asks "What's the text?" and transcribes in *Siri's*
  language, ignoring Dictate Text's setting. With Siri in English, "roxo" arrived as "Who";
  "Purple" and "Red 50%" worked. So with Siri, say the English words.
- **Started without Siri** (tap, Back Tap, Action Button): Dictate Text uses its own language,
  so Portuguese works ("Roxo" arrived as "Roxo").
- "run" before the name helps Siri treat it as a shortcut.

## From the Garmin watch (Forerunner 255)

The Connect IQ app in `garmin/carlights/` (shown as **Honda City**, a silver City with magenta
underglow as its icon) talks to `GET /say` and `GET /state` through Garmin Connect on the iPhone.
Plain `http` to the hotspot works through Garmin Connect on iOS (tested 2026-10-22); the deck has
to be on the hotspot, not home WiFi ("No answer" means it isn't).

- **Pages** (UP/DOWN, looping; opens on BOTH, so each zone is one press away): BOTH shows inside
  (left) and outside (right) as two columns, each filled with its colour up to its brightness;
  INSIDE and OUTSIDE fill the whole round face that way. On opening, the app reads both lights
  from `/state`, so the pages show the real colours.
- **START** opens that page's list: the eight colours (each with a dot), **Brightness**, **Off**.
  A colour or Off is sent for that zone (BOTH: both lights). Brightness opens a level screen whose
  fill rises and falls with UP/DOWN in 10 % steps; the deck gets the level 0.5 s after the last
  press.
- **PRESETS page** (fourth page): the deck's four presets, names with their two dots. START opens a
  list by name ("No presets yet" if the deck didn't answer); picking one applies it.
- **Not connected**: when a light has no link, its page's bottom line says "Not connected" (on
  BOTH: "Inside not connected", "Outside not connected", or "Not connected" for both) in orange in
  place of the "START: change" hint. The page still shows that light's last known colour. The app
  reads this from `/status` when it opens.
- **Glance**: a card for the watch face's glance loop with both lights' dots and levels, as of
  the last time the app ran (a glance can't make web requests). START on it opens the app.
- Requests go one at a time from a short queue; every reply is read back into the screen, so it
  shows what the deck reports. Errors: "No answer", "Phone not connected", "Wrong key".
- Key: copy `source/Secrets.mc.example` to `source/Secrets.mc` (git-ignored) with REMOTE_TOKEN.
- Build: Connect IQ SDK + a developer key, `monkeyc -d fr255 -f monkey.jungle -o bin/CarLights.prg
  -y <key> -l 2` (the manifest also lists fr255m, fr255s, fr255sm).
- Install: copy `bin/CarLights.prg` to the watch's `GARMIN/APPS` over USB (if Windows asks about
  the file already there, keep the new one), then unplug. To have it in the glance loop, add
  Honda City in the watch's glance settings.

## Updates over WiFi

The panel takes new firmware over WiFi, like the black deck (ArduinoOTA on port 3232, the password
is `REMOTE_TOKEN`; off without one).

- **Once over USB**: `pio run -e ledpanel -t upload`. This build switches the panel to a flash
  layout with two app slots; the settings (touch calibration, looks, trip, the controller's bond)
  stay, as the layout keeps them where they were.
- **Then from a PC on the iPhone hotspot**: `pio run -e ledpanel_ota -t upload`. It sends to the
  panel's hotspot address, `172.20.10.11`; from the home 5 GHz network the panel can't be reached.
  The panel listens once it is online.
- **On the screen**, in the current theme: `>UPDATING` with a 10-cell block bar and the %, then
  `>REBOOTING` and the restart into the new firmware. Meanwhile the lights stay as they are (no
  reconnects, no controller scan), the lyrics and weather polls stop, and the touch, the controller
  and voice wait (`/say` answers busy). A failed or cut upload shows `>UPDATE FAILED` in the alert
  colour (red; white on the red, orange and pink themes) for about 3 s, then the screen that was
  showing; the running firmware stays.
- **Rollback**: new firmware proves itself once it has run a minute, is online and listens for
  updates. If it crashes three times in a row before that, the panel boots the previous firmware
  (`[ota] rolled back` in the serial log). A power cut doesn't count as a crash.

## Notes

- At home the deck may join home WiFi instead (the iPhone only shows its hotspot while the
  Personal Hotspot screen is open), and then the iPhone can't reach it: open that screen and
  restart the deck. It stays on the network it joined; in the car only the hotspot exists.

- The iPhone's Bluetooth must be off for the deck to hold the LED controllers, but the hotspot is
  WiFi, so the Shortcuts still reach the deck.
- At home on the router the PC couldn't reach the deck at all (it never showed in the PC's ARP
  table), so test over the hotspot.
- Next idea: when the deck answers "Sorry", the Shortcut asks Claude to turn the phrase into a
  command ("something cozy" → "purple 30") and sends that instead.
