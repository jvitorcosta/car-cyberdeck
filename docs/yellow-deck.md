# Yellow deck

The yellow deck is a Cheap Yellow Display (ESP32-2432S028R, build `ledpanel`) that runs the car's
two LED controllers (inside and outside) over Bluetooth LE. It shows a terminal-style dashboard,
sets the lights by touch, a PS3 controller or the Garmin watch, switches the black deck's karaoke
view on and off, and in music mode gives the lights each song's colour. That is all it does: the car's
health belongs to the OBD deck, and the deck takes new firmware over USB only (its heap goes to
the two Bluetooth stacks, see [Memory](#memory)).

- [Set up](#set-up)
- [On the deck](#on-the-deck)
- [Karaoke](#karaoke)
- [Presets](#presets)
- [Music mode: song colours](#music-mode-song-colours)
- [Theme: the inside light's colour](#theme-the-inside-lights-colour)
- [Night](#night)
- [Voice and HTTP](#voice-and-http)
- [From the watch](#from-the-watch)
- [Memory](#memory)

## Set up

The steps are in the README's [Yellow deck quick start](../README.md#yellow-deck): find the
controllers' addresses with `blescan`, fill `src/config.h` (`LED_INSIDE_MAC`, `LED_OUTSIDE_MAC`,
`WIFI_NETWORKS`, `TIMEZONE`, `REMOTE_TOKEN`, `BLACK_DECK_ADDR`), flash
`pio run -e ledpanel -t upload` over USB (every update goes this way), then:

- **Touch calibration.** The first boot shows "Touch calibration": tap the five crosses with the
  pen. The fit is kept in NVS, through reboots and updates. Hold **BOOT** at power-up, or for 2 s
  at any time, to run it again.
- **PS3 controller** (DualShock 3, Bluetooth Classic): it connects to the address stored in it,
  so write the deck's there once over USB (the boot log's `[pad] deck address`, with
  `tools/ds3_pair.py <address>`; the controller must be on a libusb-compatible driver such as
  libusbK). Then press **PS**: player LED 1 lights when it's connected.
- **The phone's Bluetooth off.** A controller holds one link at a time: if the phone's app (or
  anything else) is connected to it, the deck can't reach it. The hotspot is WiFi, so the watch
  still reaches the deck.
- **Without `REMOTE_TOKEN`** the deck never joins WiFi: no clock, karaoke, music mode, `/say` or
  watch. The lights, touch and the PS3 controller still work.

Address: on the iPhone hotspot the deck is always `172.20.10.11` (elsewhere
`cardeck-leds.local`). It reads the black deck at `BLACK_DECK_ADDR`, or by its mDNS name off the
hotspot.

## On the deck

The deck boots by typing the welcome (`WELCOME TO CYBERDECK`, about half a second) and its firmware
version (`FW 2.0.0`), then shows the **main screen**. The look is "Seegson" (Alien: Isolation):
pale green on black, one hue at three brightnesses, and red kept for problems only (`-- NO LINK`,
`>OFFLINE`, `>WRONG KEY`). Every screen has faint scanlines and all-caps
labels. By day every colour clears a contrast on black of at least 4.5:1 (WCAG; at night see
[Night](#night); `src/util/yellow/termpal.h` lists each one's figure).

- **Main screen**, top to bottom; it puts first what the car's panel and CarPlay don't show:
  - the header `HONDA CITY TOURING`, with the PS3 controller's battery at the right;
  - the clock big with the date under it (`FRI 23 OCT`; `--:--` until WiFi has synced the
    clock), and on the right the trip's time big (`0:16`, hours:minutes since the deck started)
    over `TRIP` (dim);
  - both lights on one row: `IN`, a chip of its colour, its name and level (`ROSE 40%`) on the
    left, `OUT` the same on the right; `OFF` (dim) for a light that is off, `-- NO LINK` (red)
    when the deck has no link to it;
  - the prompt line: `>` and a block cursor; a message while one shows, and music mode's line
    while it is on;
  - three big buttons: `[LIGHTS]` opens the Lights page; `KARAOKE` and `MUSIC MODE` are on/off
    switches, each with an inverted `ON` / `OFF` tag under it. KARAOKE shows the black deck's
    lyrics on its own screen (see [Karaoke](#karaoke)). The deck shows no song or lyrics itself.
- **Lights**: `< BACK` returns to the main screen. Tabs for BOTH / INSIDE / OUTSIDE / PRESETS,
  then the status line. On BOTH, INSIDE and OUTSIDE: the 2x4 colour boxes (the chosen one is
  inverted) and a 10-cell level bar with `-` / `+` (labelled with the %, `MIX` or `OFF`). Tap a
  tile to set the colour and turn on; double-tap the lit one to turn off. On **PRESETS**: the six
  presets as big buttons, two rows of three, each its name over its inside and outside chips; a
  tap applies it (`>ROSE APPLIED`). The page last shown is kept over a reboot, and `[LIGHTS]`
  opens on it.
- **BOTH while the lights differ** (after a preset like ROSE, or after the INSIDE / OUTSIDE
  pages): no tile is ringed and the level box says `MIX`. On/off and `-` / `+` act on each
  light's own look; a spoken level ("50%") sets both to that level and keeps each colour; a colour
  sets both lights to it.
- **Status line**: `LINK 2/2` while the deck holds both lights; when one is down, in red,
  `OUTSIDE: NO LINK`, `INSIDE: NO LINK`, or `NO LINK` for both. What the deck shows as on is what
  it last sent while connected: the controllers can't be read back.
- **Messages**: a terminal line on the prompt line (on Lights: in place of the status line) for
  2.5 s when something happens: `>ROSE APPLIED`, `>INSIDE: NO LINK`, `>OUTSIDE: LINKED`,
  `>MUSIC ON`, `>KARAOKE ON`, and at boot the trip before (`>LAST TRIP 12.4 KM 0:47`; 6 s).
- **Taps**: a pressed button is the solid bar with a ring cut into it. After a screen or page
  change the deck waits for the finger to lift and a quarter second to pass, so a double tap on
  `[LIGHTS]` can't press a button on the next screen.
- **PS3 controller**: the D-pad moves the focus (an inverted bar on the main screen's buttons, a
  ring on Lights), **cross** presses (on KARAOKE or MUSIC MODE it flips the switch), **circle** goes back to the main screen. On Lights only:
  **square** next page (BOTH, INSIDE, OUTSIDE, PRESETS, round again); on the look pages
  **triangle** this page's lights on/off and **L1 / R1** `-` / `+`. The focus shows only while a
  controller is connected.
- **PS3 shortcuts**, on any screen, for both lights: **START** on / off, **L2 / R2** the previous /
  next preset, **SELECT** music mode on / off, **L1 / R1** `-` / `+` (on a look page they stay that
  page's), **PS** back to the main screen. The controller buzzes once when a shortcut went out, twice
  when a light isn't connected. L2 and R2 count past half travel (their pressure, so a resting
  finger does nothing; each press logs it: `[pad] trigger pressure L2 255, R2 0`). Its LEDs: **1** connected to the deck (always lit while it is),
  **2** the inside light connected, **3** the outside one, **4** its own battery low. Its battery shows at the top right of the main screen ("PAD" and four
  cells; red when it's dying, filling round while it charges).

## Karaoke

`KARAOKE` on the main screen is a switch for the black deck's karaoke view, like `MUSIC MODE`: a
tap, or cross on the PS3 controller while it has the focus, turns it on or off; the tag under the
button says `ON` or `OFF` and the prompt line shows `>KARAOKE ON` / `>KARAOKE OFF`. By voice or from
the watch, say "karaoke" (on) or "karaoke off" ("karaokê" and "caraoquê" work too); the reply is
`Karaoke on` / `Karaoke off`. It is never kept over a reboot: the deck boots with it off.

The yellow deck draws no lyrics. While the switch is on it asks the black deck `GET /karaoke?on=1`
(signed) at once and then every 10 s; the black deck turns karaoke off 30 s after the last ask, and
a tap to turn it off sends `on=0`, which stops it at once. What the black deck shows is in the
[README](../README.md#black-deck) and [ARCHITECTURE.md](ARCHITECTURE.md#black-deck). Karaoke and
music mode are independent: both can be on together.

## Presets

Six fixed presets, in the firmware (nothing to save on the deck). Each sets **both** lights on,
one colour at 40 % inside and 100 % outside:

| # | Name | Inside | Outside |
|---|---|---|---|
| 1 | ROSE | pink 40 % | pink 100 % |
| 2 | FIRE | red 40 % | red 100 % |
| 3 | GOJIRA (ゴジラ on screen) | cyan 40 % | cyan 100 % |
| 4 | PURPURA | purple 40 % | purple 100 % |
| 5 | CIVIC | green 40 % | green 100 % |
| 6 | OCEAN | blue 40 % | blue 100 % |

GOJIRA is Godzilla's atomic breath. Its name shows as **ゴジラ** on the deck's PRESETS tab and in
`>ゴジラ APPLIED`: VT323 has no kana, so the three glyphs are 16 px Unifont bitmaps
(`tools/gen_gojira_glyphs.py` → `src/yellow/fonts/gojira16.h`, drawn by `src/yellow/ui/gojiraname`).
On the wire (`/presets`, `/say`'s replies) and in the logs it is `GOJIRA`; the watch shows the
same glyphs as a bitmap.

**Cyan** is GOJIRA's colour and a preset's only: `#0082FF` on the screen, LED mix `0,130,255`.
It is not on the Lights page's grid, and song colours never snap to it. Voice sets it ("cyan",
"light blue", "azul claro"); the deck, `/say`'s replies, `/state`, `/presets` and the watch all call
it Cyan. With cyan on the inside light the terminal takes the blue theme.

- **Apply**: tap a preset on the PRESETS tab, or cross on the PS3 controller. The lights fade to it: the inside
  over about a second, the outside in two steps a second apart (its controller is slow).
- **Voice**: "rose", "fire", "gojira" (or "godzilla"), "purpura", "civic", "ocean", or "preset
  one" … "preset six" (also `favourite` / `favorite`, and digits: "preset 2"). It applies to both
  lights and ignores the rest of the phrase ("rose 50" is just ROSE). The reply is e.g.
  `Rose: inside Pink 40 percent, outside Pink 100 percent`.
- **Watch**: the PRESETS page (see [garmin/README.md](../garmin/README.md)).

**Welcome:** the first time each light links after the deck starts, a lit light plays a short
intro in your last look. The inside light goes dark, then its strip fills with colour from both ends
inwards over 4 s and takes your look. The outside light stays dark meanwhile, then rises through a
peak (30 points brighter, at most 100 %) as the fill ends and settles on its look, about 6 s in all.
The deck can't light single LEDs: the fill is the inside controller's own built-in curtain effect
(`ledintro::FILL_MODE` and `FILL_SPEED`), started with its effect command, and the outside
controller has no such effect. The curtain's colours are the controller's own, so for a look it
has no colour for (orange, pink) the strip fills with the nearest and shifts to the look at the
end. Each light ends on its look twice, so a lost write still lands on it. A light you left off
stays off, music mode skips the intro, and a change, a preset or a song's colour takes over at
once. A later reconnect sets the look straight away.

Tuning the fill in the car: the effect numbers come from published notes on a sibling controller
(MELK-OA10), which list 57 to 76 as the curtains without naming each one. If the inside strip
doesn't fill from both ends inwards, try the other numbers in that range in `FILL_MODE`; if the
fill doesn't finish in time, raise `FILL_SPEED` (0 to 100) or `FILL_MS`.

Checking it on the desk: `python tools/intro_check.py <port>` sends `INTRO` over USB, which plays the
welcome again on each lit light without a reboot, and holds the deck's `[intro]` log (each key,
its time, `sent` or `no link`) to the timeline above: PASS or FAIL, and the timeline. In the car,
the same command (or a reboot) plays it on the lights, and `>WELCOME` on the prompt line says it
ran. It only checks the deck: how the controller draws the fill is seen in the car.

## Music mode: song colours

`MUSIC MODE` on the main screen (a tap, or cross on the PS3 controller), the voice word "music" ("music off" to
stop) or the watch's **Music mode** item turns it on or off; the tag under the button says `ON`
or `OFF`, and `>MUSIC ON` / `>MUSIC OFF` shows on the prompt line. It is never kept over a
reboot. There is no microphone: the black deck picks the current song's colour (one of the grid's
8, see [Which colour](#which-colour)) and sends it over WiFi.

- **Colour**: both lights take the song's colour with that colour's tuned LED mix, steady through
  the song. On a new song they fade to the next colour over about 3 s. The black deck waits for
  the new song's cover (up to 8 s) before it sends, so a song changes the lights once.
- **Brightness**: the outside light runs at 100 % at any hour. The inside light stays at the level
  you left it, times the time-of-day cap (day 06:00-18:00 100 %, evening 18:00-22:00 and 05:00-06:00
  70 %, late night 22:00-05:00 45 %), but never under 40 %; at 40 % or less it keeps its level. At
  each step a playing song's inside light fades to the new level once.
- **Off lights**: an outside light you left off stays off; the inside light is switched on.
- **Screen**: the terminal theme follows the song's colour. The lights row shows what the lights
  show; the prompt line shows `>MUSIC [chip] PINK`, `>MUSIC: WAITING` until the first palette
  comes, and `>MUSIC: NO SIGNAL` when none came for 15 s (the lights keep the last colour).
- **Ending it**: any other change from anywhere (a colour, preset, level or on/off by touch, the
  PS3 controller, `/say` or the watch) ends the mode, then applies to your own looks. Turning the
  mode off brings back the looks from before it started; they are never saved over while it
  runs, so a reboot brings them back too.
- **`/state`, `/status` and the watch**: while the mode drives the lights, `/state` reports what
  they show, and `/status` says `music=on`.
- **Between the decks**: the yellow deck asks the black deck `GET /music?on=1` (signed) every 10 s
  while the mode is on, `on=0` when it ends; the black deck stops on its own 30 s after the last
  ask. Until its clock is set the yellow deck asks nothing. The palettes come over UDP port 4210,
  signed (`src/util/wire/musicwire.h`), and the yellow deck drops any packet that isn't from the
  black deck's proven boot or doesn't count up. `GET /music/debug?k=<token>` on the black deck
  shows the palette it last sent, its cap and the socket.

### Which colour

The black deck decides (`src/util/black/songmood`), strongest signal first. Text matches ignore
case and accents, and titles and albums also match without a suffix such as ` - Remastered` or
` (feat. ...)`:

1. **Your picks** always win: a song (title and artist), an album or an artist, each to a colour
   (a song's pick beats its album's, which beats its artist's). They live in
   `src/util/black/songpicks.inc`; add a line and rebuild the black deck:
   `{"Artist", "Song title", nullptr, RED},` for a song, `{"Artist", nullptr, "Album", PINK},` for
   an album, `{"Artist", nullptr, nullptr, BLUE},` for every song of an artist. Colours: `RED`,
   `ORANGE`, `PINK`, `PURPLE`, `BLUE`, `GREEN`, `WHITE`, `YELLOW`.
2. **A colour word in the song's title** (whole words): purple/púrpura/roxo/violeta,
   red/vermelho/vermelha, blue/azul, pink/rosa, green/verde, yellow/amarelo/gold/dourado/ouro,
   orange/laranja, white/branco. Artists' names don't count.
3. **Your playlist's colour tags**: write them anywhere in a playlist's description on Spotify.
   `#white` makes every song in it white; `#red #pink #purple` makes those its colours (the cover
   chooses among them, as for a mood below). Tags: #red/#vermelho/#vermelha, #orange/#laranja,
   #pink/#rosa, #purple/#roxo/#roxa/#púrpura/#violeta, #blue/#azul, #green/#verde,
   #white/#branco/#branca, #yellow/#amarelo/#amarela/#gold/#dourado (any case, accents or not;
   `#redemption` is no tag). The deck reads the description when it looks the playlist up, so new
   tags apply after you play the playlist from another context first, or after a restart. Liked
   Songs has no description.
4. **A colour word in the album, then the playlist's name** (not for a playlist with tags).
5. **A mood** allows a few colours, and the **cover** chooses among them. The mood comes from the
   playlist (its name, then its description: words and emoji), else from the genre, else all 8:

   | Mood | Playlist words / emoji | Colours |
   |---|---|---|
   | neon | night noite neon lights city midnight madrugada dark 🌃 🌆 🌙 🌌 | red, purple, blue, pink |
   | intimate | love amor cuddle(s) moon lua romance romantic kiss slow sexy ❤️ 💕 💜 🌹 🌙 | red, pink, purple |
   | drive | drive racing race speed hamilton f1 formula fast furious carro estrada road 🏎️ 🏁 🚗 | red, orange, blue, yellow |
   | energy | workout treino gym academia run corrida hype party festa baile 🔥 💪 🎉 | red, orange, pink, yellow |
   | calm | chill relax calm calma sleep sono study estudo lofi lo-fi acoustic rain chuva 🌧️ ☕ 🌊 | blue, white, purple |
   | sunny | verão summer praia beach sol sun tropical ☀️ 🏖️ 🌴 | yellow, orange, green |

   Genres (the first colour is a grey cover's):

   | Genre | Colours |
   |---|---|
   | R&B | purple, red, pink |
   | pop, K-pop, J-pop | pink, purple, blue |
   | rap | red, purple, blue |
   | funk, afro | red, purple, pink |
   | funk BR | pink, purple, red |
   | rock, metal | red, orange |
   | alt | blue, purple, red |
   | electro, dance | blue, purple, pink |
   | Brazilian (brasil, sertanejo, MPB, samba, pagode, forró, axé) | yellow, orange, green, blue |
   | reggae | green, yellow, red |
   | latin | orange, yellow, red |
   | jazz, gospel, classical, country, folk | blue, white, orange |
   | soundtracks | blue, purple, white |
   | anything else | all 8 |

   The first mood in the table that matches wins (so 🌙 is neon); a song from an album, an artist
   or Liked Songs has no playlist, so its genre decides.

   The cover's colours, strongest first: the first whose nearest colour is allowed wins. If none
   is, the allowed colour nearest the cover's main colour if within 60° of hue. Failing that (a
   black-and-white cover too): under a playlist's tags or mood, the first colour of the song's
   genre list that the playlist allows; with no overlap or no genre, the mood's first colour. A
   white-ish cover colour counts as white only where white is allowed.

The time of day changes only the brightness, never the colour. The black deck's serial log has one
line per song with what it knew and which rule decided:
`[colour] "Popular" | The Weeknd | Popular | "blinded by the lights 🌃" | genre R&B | cover FF0800 FF9000 | playlist mood neon | -> Red cap 100`.

## Theme: the inside light's colour

While the **INSIDE** light is on, the terminal (main screen and Lights; not the boot typing
or the touch calibration) takes that light's colour: the same three brightnesses in
its hue. Blue and purple are lighter shades so the text reads; white is a soft grey-white, held
back so it doesn't glare. Problems stay red, except on the red,
orange and pink themes, where they show in white. The colour tiles and chips keep their real
colours. Cyan takes blue's theme. The theme follows the inside look as the deck has it, linked or
not: a colour tapped with the lights out of reach still shows on the screen. With the inside light
off the screen goes back to green. To force a theme whatever the look, build with `THEME_TEST`
(0..7: red, orange, pink, purple, blue, green, white, yellow), then clear it:

```
$env:PLATFORMIO_BUILD_FLAGS="-D THEME_TEST=6"; pio run -e ledpanel -t upload
Remove-Item Env:PLATFORMIO_BUILD_FLAGS
```

## Night

From **18:00 to 06:00** local time (`TIMEZONE`), once the deck has the time over WiFi, the
backlight drops to about a third (85 of 255) and the screen takes the **night palette**: each theme
in its own hue at full saturation, so the colours stay vivid on the dimmed panel (the day's pale
tints went grey there). The colour chips and the lights are untouched. Without WiFi there is no
clock, so it stays day. The serial log says `[ui] night on` / `off`.

Picked on the panel (TEXT / MID / DIM, then problems):

| Theme | Night colours | Problems |
|---|---|---|
| red | `FF0000` `E00000` `BC0000` | white |
| orange | `FF4600` `E03400` `BC2800` | white |
| pink | `FF0040` `E00038` `BC0030` | white |
| purple | `D200FF` `B800E0` `9A00BC` | red |
| blue | `0046FF` `003CE0` `0032BC` (marine, like the inside light) | red |
| green (and no theme) | `50FF46` `46E03C` `3ABC32` (phosphor terminal) | red |
| white | `FFFFFF` `EBEBEB` `D2D2D2` | red |
| yellow | `FFB400` at all three levels | red |

What the panel taught: it pushes green, so the red, orange and pink carry little or none (a touch
turns red orange and orange yellow), and blue turns pink purple; a darker yellow reads olive, so
the yellow stays one colour. A pure red or blue can't reach the day's contrast, so night asks
TEXT >= 3:1 and DIM >= 2:1 (`termpal.h`, `test/test_termpal`).

To check it without waiting for evening, build with the `NIGHT_TEST` hook, then clear it:

```
$env:PLATFORMIO_BUILD_FLAGS="-D NIGHT_TEST"; pio run -e ledpanel -t upload
Remove-Item Env:PLATFORMIO_BUILD_FLAGS
```

That build also takes a line on the serial port (115200; opening it with DTR/RTS low keeps the CYD
from resetting) to try colours live, no rebuild:

| Line | Does |
|---|---|
| `N <theme -1..7> <TEXT> <MID> <DIM> [ALERT]` | that theme's night colours (hex `RRGGBB`), and shows it |
| `T <theme -1..7>` | shows that theme (`T` alone the next one, `T X` back to the inside light's) |
| `B <0..255>` | the night backlight |
| `D` | day / night, to compare |
| `P` | prints every theme's night colours and the backlight |

A screen capture (`tools/deck_shot.py`) shows the RGB but not the backlight: judge on the panel.

## Voice and HTTP

The deck's web server (port 80) answers four routes, all `GET`, all plain text:

| Route | What it does | Reply |
|---|---|---|
| `/say?text=<phrase>` | changes the lights (see [What it understands](#what-it-understands)) | `Purple, 70 percent`, `Rose: inside Pink 40 percent, outside Pink 100 percent`, `Music mode on` |
| `/state` | the current look, changes nothing | `Purple, 70 percent`, `Lights off` |
| `/presets` | the six presets, one line each: slot, name, inside colour and level, outside colour and level | `1 ROSE pink 40 pink 100` |
| `/status` | each light's link, music mode, the firmware | `inside=on outside=nolink music=off fw=2.0.0` |

- `/say` and `/state` take `&zone=inside|outside`: then only that light changes (or is read), and
  the reply is prefixed (`Inside: Purple, 70 percent`). Without it, both lights (the BOTH page).
  Spoken zone words are ignored. The page on the deck's screen stays.
- A voice reply says the level only when the command changed it (`Purple` after "purple",
  `Purple, 50 percent` after "purple 50"); `/state` always does.
- A change for a light the deck has no link to says so: `Purple, outside not connected`
  (`inside not connected`, `lights not connected`). In `/status` a light is `on`, `off` or
  `nolink`; a light the deck has no link to is always `nolink`.
- **The key**: each route takes either `k=<REMOTE_TOKEN>` (for a plain link) or a
  signature, `t=<unix s>&n=<16 hex>&sig=<32 hex>`: the first 16 bytes of HMAC-SHA256 with
  `REMOTE_TOKEN` over `GET|<path>|<params>|<t>|<n>`, params sorted by name and percent-encoded
  (`src/util/wire/sigreq.h`). The watch signs. A signed request must be within 2 minutes of the
  deck's clock (else 403 `clock off`) and is accepted once; before the deck's first SNTP answer it
  gets 503 `clock not set` (the `k=` still works). See [SECURITY.md](../SECURITY.md).
- Errors: 403 `Wrong key`; `Sorry, I didn't get a colour or a command` for a phrase with nothing
  to do; 503 `The deck is busy, try again` while the screen can't take
  it (calibrating).

### What it understands

English or Portuguese, any order, other words ignored (`src/util/yellow/ledvoice.h`):

| Kind | Words |
|---|---|
| Colours | red/vermelho, orange/laranja, pink/rosa, purple/roxo/lilás, blue/azul, green/verde, white/branco, yellow/amarelo (also "read" = red, "blew" = blue, as a phone's dictation hears them); cyan/ciano, "light blue" (the two words together), azul claro/clara |
| Power | on, off / liga, desliga, acende, apaga |
| Brighter / dimmer (10 % steps) | brighter, up, more / mais, aumenta, forte; dimmer, down, less / menos, diminui, fraco |
| Level | a number (`50`, `50%`, rounded to 10), max/máximo, min/mínimo |
| Preset | rose, fire, gojira/godzilla, purpura, civic, ocean; or preset/favourite/favorite + 1-6, as a word or digit ("preset two", "preset 2") |
| Karaoke | karaoke, karaokê, caraoquê: on; with an off word ("karaoke off"): off. Nothing else in the phrase is applied |
| Music mode | music, música ("music mode" too): on; with an off word ("music off", "desliga o modo música"): off. Nothing else in the phrase is applied |

A colour also turns the lights on. "light" only counts right before "blue" ("light off" is just
off), so "turn the light blue" means cyan, and "lights blue" means blue.

## From the watch

The **Honda City** Connect IQ app (Forerunner 255) shows both lights, changes each zone's colour
and level, applies the presets and turns music mode on and off, through Garmin Connect on the
iPhone. It signs every request. How it works, and how to build and install it:
[garmin/README.md](../garmin/README.md).

## Memory

The deck runs two Bluetooth stacks at once (Bluedroid's dual mode: Classic for the PS3 controller,
LE for the lights), which leaves the least heap of any build. So it does lights, the karaoke switch, music
mode and the watch's routes, and nothing else: no weather, speed cameras, car health or updates
over WiFi (the car's health is the OBD deck's). It still keeps two app slots
for the boot guard: three crashes in a row before a firmware has run a minute online boot the
other slot (`[ota] rolled back`); `[ota] this firmware is good` marks it proven.

Taking the karaoke lyrics, the `/state` poll of the black deck, the Menu's now-playing band and the
Japanese font off the deck moved its free heap from ~31-33 KB (low-water 25.7 KB, largest block
11.7 KB) to ~44-52 KB (low-water 41 KB, largest block 27.6 KB). The one task left that talks to the
black deck, `deck` (5 KB of stack), only sends the music and karaoke keep-alives.

Every 10 s the serial log prints the free heap, its low-water mark, its largest free block, the
allocations that failed and each task's unused stack (`[mem] heap 44500 (min 41000, block 27600,
fails 0)  stack left: link 3568, voice 2472, deck ..., ...`, from a desk run with WiFi up). If
a feature comes back, watch the `min` and the `block`: lwIP aborts when a socket can't get its
semaphore, and a library that takes one big buffer fails first on a heap in pieces.

Why it last restarted is kept across the restart: each boot prints `[boot] reset reason`
(power-on, panic, task watchdog, brownout...) and, after a reset, `[crash] last run:` with how long
that run lasted, its heap in its last second, the allocations that failed and what it was doing
(`at state get`, `led write`, `pad press`, `music ask`). The last six stay in NVS (`[crash] 1:` ..
`6:` at each boot), so after a restart in the car plug the deck in at home and read the boot log.
A brownout is the car's power, not the firmware.

The radio prefers Bluetooth over WiFi (`esp_coex_preference_set`): on the default balance the PS3
controller's reports came through ~20 times a second and quick taps went missing; `[pad] 45
reports/s` every 10 s tells how it's doing.

## Notes

- At home the deck may join home WiFi instead of the hotspot (the iPhone only shows its hotspot
  while the Personal Hotspot screen is open), and then the iPhone can't reach it: open that screen
  and restart the deck. It stays on the network it joined; in the car only the hotspot exists.
