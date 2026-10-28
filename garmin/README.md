# Honda City watch app

`carlights/` is a Connect IQ watch app for the Garmin Forerunner 255 (also 255M, 255S, 255S
Music) that controls the [yellow deck](../docs/yellow-deck.md)'s car lights from the wrist. It
shows as **Honda City**, with a silver City with magenta underglow as its icon.

The watch faces (Pokéface) live in their own repository,
[jvitorcosta/garmin-watchfaces](https://github.com/jvitorcosta/garmin-watchfaces).

## How it reaches the deck

The watch has no WiFi of its own here: every request goes through Garmin Connect on the iPhone,
which is on the same hotspot as the deck. The app calls the yellow deck at `http://172.20.10.11`
(its fixed address on the iPhone hotspot) with `GET /say`, `/state`, `/presets` and `/status`
([the routes](../docs/yellow-deck.md#voice-and-http)). Plain `http` to the hotspot works through
Garmin Connect on iOS. The deck has to be on the hotspot, not home WiFi: "No answer" means it
isn't.

Every request is **signed** with `REMOTE_TOKEN` (`source/Sign.mc`: HMAC-SHA256 with
`Toybox.Cryptography`, the watch's clock and a random nonce, the same scheme as the decks'
`src/util/wire/sigreq.h`); the key itself never leaves the watch. Requests go one at a time from
a short queue, and every reply is read back into the screen, so it shows what the deck reports.

## Using it

- **Pages** (UP / DOWN, looping; it opens on BOTH, so each zone is one press away): BOTH shows the
  inside (left) and outside (right) lights as two columns, each filled with its colour up to its
  brightness; INSIDE and OUTSIDE fill the whole round face that way. On opening, the app reads both
  lights from `/state`, the presets from `/presets` and the links and music mode from `/status`.
- **START** opens that page's list: the eight colours (each with a dot, in the deck's grid order),
  **Brightness**, **Off**, and on BOTH **Music mode** (its sublabel says On or Off). A colour or Off
  is sent for that zone (BOTH: both lights). Brightness opens a level screen whose fill rises and
  falls with UP / DOWN in 10 % steps; the deck gets the level 0.5 s after the last press. Music
  mode turns the deck's music mode on, or off when it is on; any other change sent from the watch
  ends it, as it does on the deck.
- **PRESETS** (the fourth page): the deck's six presets as bars, the name over each. The left half
  is the inside light, the right half the outside one, each filled from its left edge to its level
  (a light that's off says OFF). START opens the same bars as a list ("No presets yet" if the deck
  didn't answer); START on one applies it, buzzes, says APPLIED for a second and returns to the
  pages; BACK leaves without applying. GOJIRA's bar shows its name as **ゴジラ**, a small bitmap of
  the deck's Unifont glyphs (the watch's fonts have no kana).
- **Colours** are the deck's screen colours, except purple (`AA00FF`, the deck's `#8C00FF` as the
  watch's 64 colours show it) and GOJIRA's cyan (`00AAFF`, apart from OCEAN's blue and from green).
  Cyan comes only from the preset: a zone list opened on a cyan light starts on Blue.
- **No link**: when the deck has no link to a light, that page's bottom line says "No link" (on
  BOTH: "Inside: no link", "Outside: no link", or "No link" for both) in orange in place of the
  "START: change" hint. The page still shows that light's last known colour.
- **Glance**: a card in the watch's glance loop with both lights' dots and levels (`IN 40%`,
  `OUT OFF`) as of the last time the app ran (a glance can't make web requests). START on it opens
  the app. To have it there, add Honda City in the watch's glance settings.
- **Errors** on the bottom line: "No answer" (the deck isn't on the hotspot), "Phone not connected"
  (no Bluetooth link to Garmin Connect), "Wrong key" (`Secrets.mc` doesn't match the deck's
  `REMOTE_TOKEN`), "Clock off: sync watch" (the watch's clock is more than 2 minutes off the
  deck's), "Deck has no clock" (the deck has had no SNTP answer yet), "Deck busy" (the deck is busy,
  for example updating), or the deck's own "Sorry ..." line.

## Build and install

1. Install the [Connect IQ SDK](https://developer.garmin.com/connect-iq/sdk/) (the SDK Manager)
   with the Forerunner 255 device files, and make a developer key once:
   `openssl genrsa -out developer_key.pem 4096` then
   `openssl pkcs8 -topk8 -inform PEM -outform DER -in developer_key.pem -out developer_key.der -nocrypt`.
   Keep the key outside the repository.
2. Copy `carlights/source/Secrets.mc.example` to `carlights/source/Secrets.mc` (git-ignored) and
   put the decks' `REMOTE_TOKEN` in it.
3. Build, from `garmin/carlights/`:

   ```sh
   monkeyc -d fr255 -f monkey.jungle -o bin/CarLights.prg -y <path to developer_key.der> -l 2
   ```

   (`-d fr255m`, `fr255s` or `fr255sm` for the other models; on Windows the command is
   `monkeyc.bat`.) `bin/` is git-ignored.
4. Install: plug the watch in over USB, copy `bin/CarLights.prg` to its `GARMIN/APPS` folder (if
   Windows asks about the file already there, keep the new one), then unplug.

The signing has known-answer tests (`source/SignTest.mc`, the same vectors as `test/test_sigreq`):
build with `--unit-test` and run them in the simulator with `monkeydo bin/CarLights.prg fr255 /t`.

## Source map

| File | What it does |
|---|---|
| `CarLightsApp.mc` | the app: the pages and the glance |
| `ZonesView.mc`, `ZonesDelegate.mc` | the four pages, their lists (colours, Brightness, Off, Music mode; the presets) |
| `LevelView.mc` | a zone's brightness screen |
| `PresetBar.mc` | a preset drawn as a bar; the Presets list's rows |
| `Fill.mc`, `ColorDot.mc` | the filled-column look and the list's colour dots |
| `Deck.mc` | the lights, presets, links and music mode as the deck reports them, and the request queue |
| `Sign.mc`, `SignTest.mc` | request signing and its tests |
| `HondaGlance.mc` | the glance card |
| `Secrets.mc.example` | the template for the git-ignored `Secrets.mc` |
