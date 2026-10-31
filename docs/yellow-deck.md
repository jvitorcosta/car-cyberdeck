# Yellow deck

The yellow deck is a Cheap Yellow Display (ESP32-2432S028R, build `ledpanel`) that runs the car's
two LED controllers (inside and outside) over Bluetooth LE. It shows a terminal-style dashboard,
sets the lights by touch, an Xbox controller, Siri or the Garmin watch, shows the black deck's
lyrics, and in music mode gives the lights each song's colour.

- [Set up](#set-up)
- [On the deck](#on-the-deck)
- [Presets](#presets)
- [Music mode: song colours](#music-mode-song-colours)
- [Theme: the inside light's colour](#theme-the-inside-lights-colour)
- [Voice and HTTP](#voice-and-http), [Siri Shortcuts](#siri-shortcuts)
- [From the watch](#from-the-watch)
- [Updates over WiFi](#updates-over-wifi)

## Set up

The steps are in the README's [Yellow deck quick start](../README.md#yellow-deck): find the
controllers' addresses with `blescan`, fill `src/config.h` (`LED_INSIDE_MAC`, `LED_OUTSIDE_MAC`,
`WIFI_NETWORKS`, `TIMEZONE`, `REMOTE_TOKEN`, `OTA_PASSWORD`, `BLACK_DECK_ADDR`), flash
`pio run -e ledpanel -t upload` once over USB, then:

- **Touch calibration.** The first boot shows "Touch calibration": tap the five crosses with the
  pen. The fit is kept in NVS, through reboots and updates. Hold **BOOT** at power-up, or for 2 s
  at any time, to run it again.
- **Xbox controller** (Series X|S, or One model 1708 and later): hold its pair button until the
  logo flashes fast. The deck takes the first controller it finds in pairing mode and reconnects
  to that one whenever it is switched on.
- **The phone's Bluetooth off.** A controller holds one link at a time: if the phone's app (or
  anything else) is connected to it, the deck can't reach it. The hotspot is WiFi, so Siri and the
  watch still reach the deck.
- **Without `REMOTE_TOKEN`** the deck never joins WiFi: no clock, weather, lyrics, music mode,
  voice, watch or updates over WiFi. The lights, touch and the Xbox controller still work.

Address: on the iPhone hotspot the deck is always `172.20.10.11` (elsewhere
`cardeck-leds.local`). It reads the black deck at `BLACK_DECK_ADDR`, or by its mDNS name off the
hotspot.

## On the deck

The deck boots by typing the welcome (`WELCOME TO CYBERDECK`, about half a second) and its firmware
version (`FW 2.0.0`), then shows the **main screen**. The look is "Seegson" (Alien: Isolation):
pale green on black, one hue at three brightnesses, and red kept for problems only (`-- NO LINK`,
`>OFFLINE`, `>WRONG KEY`). Every screen has faint scanlines and all-caps
labels. Every colour clears a contrast on black of at least 4.5:1 (WCAG;
`src/util/yellow/termpal.h` lists each one's figure).

- **Main screen**, top to bottom; it puts first what the car's panel and CarPlay don't show:
  - the header `CYBERDECK // HONDA CITY TOURING 2022`;
  - the clock big with the date under it (`FRI 23 OCT`; `--:--` until WiFi has synced the
    clock), and on the right the outside temperature big (`32°C`) and the sky in words
    (`PARTLY CLOUDY`); under them `HI 32 LO 26  <CITY>` (dim). Where the car is comes from the
    hotspot's public IP (ip-api.com, approximate), the weather from Open-Meteo, every 15 min.
    A failed fetch keeps the last reading; `>OFFLINE` shows only when no reading ever came;
  - the trip, from the OBD deck (see [ARCHITECTURE.md](ARCHITECTURE.md) "OBD deck"): three
    figures, the trip `12.4 KM`, `11.8 KM/L` now and the range `296 KM LEFT`, and under them
    `0:16  R$ 6,30  14.1V  90C` (the time since the deck started, R$ so far, battery, coolant).
    Without the OBD deck (or with no new snapshot for 3 s) the figures read a dim `--` and the
    small line `0:16  R$ --  --V  --C` (the time, dashes for the rest);
  - both lights on one row: `IN`, a chip of its colour, its name and level (`ROSE 40%`) on the
    left, `OUT` the same on the right; `OFF` (dim) for a light that is off, `-- NO LINK` (red)
    when the deck has no link to it;
  - the prompt line: `>` and a block cursor; a message while one shows, and music mode's line
    while it is on. A car warning from the OBD deck shows here in red for 2.5 s (also on the
    Lights page's status line): the engine's (`>ENGINE HOT 108C`, `>CHECK ENGINE LIGHT ON`) even
    while driving, any problem (`>BATTERY LOW WHILE RUNNING`, `>2 STORED CODES`) only once the
    car stands; each at most once every 2 minutes, never over the health screens;
  - three big buttons: `[LIGHTS]` opens the Lights page, `[LYRICS]` the black deck's lyrics,
    `MUSIC MODE` turns music mode on or off (an inverted `ON` / `OFF` tag under it). LYRICS is
    bright with a `LIVE` tag while the black deck plays a song with synced lyrics, dim otherwise:
    a tap then says why.
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
- **Lyrics**: the song the black deck plays (`TITLE - ARTIST`, in capitals), its synced line big,
  the next line under it, and a thin progress line. Accented Latin letters draw in the terminal's
  font; **Japanese** (kana and the common JIS level 1 kanji) draws from a GNU Unifont subset in
  flash (`data/jp16.bin`, `tools/gen_jp_font.py`), and a character outside it (Chinese-only
  hanzi, rarer kanji, Korean) shows as an empty box. Lines wrap at spaces, and anywhere between
  Japanese characters. With nothing to sing along it shows, big and dim, `>NOTHING PLAYING`,
  `>NO SYNCED LYRICS`, `>LYRICS: NO SIGNAL` (the black deck doesn't answer) or `>WRONG KEY` (in
  red: the two decks' `REMOTE_TOKEN` differ). The page polls the black deck every second.
- **Messages**: a terminal line on the prompt line (on Lights: in place of the status line) for
  2.5 s when something happens: `>ROSE APPLIED`, `>INSIDE: NO LINK`, `>OUTSIDE: LINKED`,
  `>MUSIC ON`, why LYRICS won't open, and at boot the trip before (`>LAST TRIP 12.4 KM 0:47`; 6 s).
- **Taps**: a pressed button is the solid bar with a ring cut into it. After a screen or page
  change the deck waits for the finger to lift and a quarter second to pass, so a double tap on
  `[LIGHTS]` can't press a button on the next screen.
- **Xbox controller**: the D-pad moves the focus (an inverted bar on the main screen's buttons, a
  ring on Lights), **A** presses, **B** goes back to the main screen. On Lights only: **X** next
  page (BOTH, INSIDE, OUTSIDE, PRESETS, round again); on the look pages **Y** this page's lights
  on/off and **LB / RB** `-` / `+`. The focus shows only while a controller is connected.

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
On the wire (`/presets`), in the logs and in Siri's replies it is `GOJIRA`; the watch shows the
same glyphs as a bitmap.

**Cyan** is GOJIRA's colour and a preset's only: `#0082FF` on the screen, LED mix `0,130,255`.
It is not on the Lights page's grid, and song colours never snap to it. Voice sets it ("cyan",
"light blue", "azul claro"); the deck, Siri's replies, `/state`, `/presets` and the watch all call
it Cyan. With cyan on the inside light the terminal takes the blue theme.

- **Apply**: tap a preset on the PRESETS tab, or Xbox A on it. The lights fade to it: the inside
  over about a second, the outside in two steps a second apart (its controller is slow).
- **Voice**: "rose", "fire", "gojira" (or "godzilla"), "purpura", "civic", "ocean", or "preset
  one" … "preset six" (also `favourite` / `favorite`, and digits: "preset 2"). It applies to both
  lights and ignores the rest of the phrase ("rose 50" is just ROSE). The reply is e.g.
  `Rose: inside Pink 40 percent, outside Pink 100 percent`. Siri may hear "purpura" as "purple",
  the colour; say "preset four" for that one.
- **Watch**: the PRESETS page (see [garmin/README.md](../garmin/README.md)).

**Welcome:** the first time each light links after the deck starts, a lit light rises from black
to your last look over about 3 s. A later reconnect sets the look straight away.

## Music mode: song colours

`MUSIC MODE` on the main screen (a tap, or Xbox A on it), the voice word "music" ("music off" to
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
  Xbox controller, Siri or the watch) ends the mode, then applies to your own looks. Turning the
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

While the **INSIDE** light is on and linked, the terminal (main screen, Lights, Lyrics; not the
boot typing or the touch calibration) takes that light's colour: the same three brightnesses in
its hue. Blue and purple are lighter shades so the text reads; white is a soft grey-white, held
back so it doesn't glare. Problems stay red, except on the red,
orange and pink themes, where they show in white. The colour tiles and chips keep their real
colours. Cyan takes blue's theme. With the inside light off or out of reach the screen goes back
to green. To see a theme without the car's lights, build with `THEME_TEST` (0..7: red, orange,
pink, purple, blue, green, white, yellow), then clear it:

```
$env:PLATFORMIO_BUILD_FLAGS="-D THEME_TEST=6"; pio run -e ledpanel -t upload
Remove-Item Env:PLATFORMIO_BUILD_FLAGS
```

## Voice and HTTP

The deck's web server (port 80) answers four routes, all `GET`, all plain text:

| Route | What it does | Reply |
|---|---|---|
| `/say?text=<phrase>` | changes the lights (see [What it understands](#what-it-understands)), or notes a full tank on the OBD deck | `Purple, 70 percent`, `Rose: inside Pink 40 percent, outside Pink 100 percent`, `Music mode on`, `Full tank noted, 6.49 per litre.` |
| `/state` | the current look, changes nothing | `Purple, 70 percent`, `Lights off` |
| `/presets` | the six presets, one line each: slot, name, inside colour and level, outside colour and level | `1 ROSE pink 40 pink 100` |
| `/status` | each light's link, music mode, the firmware | `inside=on outside=nolink music=off fw=2.0.0` |

- `/say` and `/state` take `&zone=inside|outside`: then only that light changes (or is read), and
  the reply is prefixed (`Inside: Purple, 70 percent`). Without it, both lights (the BOTH page).
  Siri sends no zone, so spoken zone words are ignored. The page on the deck's screen stays.
- A voice reply says the level only when the command changed it (`Purple` after "purple",
  `Purple, 50 percent` after "purple 50"); `/state` always does.
- A change for a light the deck has no link to says so: `Purple, outside not connected`
  (`inside not connected`, `lights not connected`). In `/status` a light is `on`, `off` or
  `nolink`; a light the deck has no link to is always `nolink`.
- **The key**: each route takes either `k=<REMOTE_TOKEN>` (Siri Shortcuts can't do better) or a
  signature, `t=<unix s>&n=<16 hex>&sig=<32 hex>`: the first 16 bytes of HMAC-SHA256 with
  `REMOTE_TOKEN` over `GET|<path>|<params>|<t>|<n>`, params sorted by name and percent-encoded
  (`src/util/wire/sigreq.h`). The watch signs. A signed request must be within 2 minutes of the
  deck's clock (else 403 `clock off`) and is accepted once; before the deck's first SNTP answer it
  gets 503 `clock not set` (the `k=` still works). See [SECURITY.md](../SECURITY.md).
- Errors: 403 `Wrong key`; `Sorry, I didn't get a colour or a command` for a phrase with nothing
  to do; 503 `The deck is busy, try again` while an update runs or the screen can't take it
  (calibrating).

### What it understands

English or Portuguese, any order, other words ignored (`src/util/yellow/ledvoice.h`):

| Kind | Words |
|---|---|
| Colours | red/vermelho, orange/laranja, pink/rosa, purple/roxo/lilás, blue/azul, green/verde, white/branco, yellow/amarelo (also Siri's "read" = red, "blew" = blue); cyan/ciano, "light blue" (the two words together), azul claro/clara |
| Power | on, off / liga, desliga, acende, apaga |
| Brighter / dimmer (10 % steps) | brighter, up, more / mais, aumenta, forte; dimmer, down, less / menos, diminui, fraco |
| Level | a number (`50`, `50%`, rounded to 10), max/máximo, min/mínimo |
| Preset | rose, fire, gojira/godzilla, purpura, civic, ocean; or preset/favourite/favorite + 1-6, as a word or digit ("preset two", "preset 2") |
| Music mode | music, música ("music mode" too): on; with an off word ("music off", "desliga o modo música"): off. Nothing else in the phrase is applied |

**Fill-up** (`src/util/car/fuelbook.h`, checked before the lights): "filled up at 6.49", "fill
up 6,49", "full tank", "tanque cheio a 6,49", "abasteci a 5,9". The deck forwards it to the OBD
deck (which keeps the fuel book) as a signed `GET /fill?p=6.49` (no `p` without a price, or with
one outside 3.00-15.00 R$/L: the tank is noted full all the same) and answers:

| Said | Reply |
|---|---|
| "filled up at 6.49" | `Full tank noted, 6.49 per litre.` |
| "filled up at 649" | `Full tank noted. I heard 649, that's not a price.` |
| "full tank" | `Full tank noted.` |
| any, OBD deck not answering (~2-3 s) | `I can't reach the car module.` |

A colour also turns the lights on. "light" only counts right before "blue" ("light off" is just
off), so "turn the light blue" means cyan, and "lights blue" means blue.

### Siri Shortcuts

**One shortcut for anything.** In the Shortcuts app, a new shortcut:

1. **Dictate Text**: language English (or Portuguese), stop listening *After Pause*.
2. **URL Encode** the Dictated Text.
3. **Text**: `http://172.20.10.11/say?k=<REMOTE_TOKEN>&text=` followed by the URL Encoded Text
   variable.
4. **Get Contents of URL** of that Text (method GET).
5. **Speak Text**: Contents of URL (or *Show Result*, for a silent version).
6. Name it, for example **Honda** ("Lights" is taken by HomeKit).

"Hey Siri, Honda" → Siri listens → "purple" → the lights turn purple and Siri says "Purple". The
first run asks to allow connecting to 172.20.10.11: allow.

**One phrase, no question.** For the commands used most, one shortcut each skips the dictation:
the same without steps 1-2, with the phrase in the URL (`...&text=purple`), named for example
**Purple lights**. Likewise "Lights off", "Music mode".

**Which language.** Tested 2026-10-21:

- **"Hey Siri, run <name>"**: Siri itself asks "What's the text?" and transcribes in *Siri's*
  language, ignoring Dictate Text's setting. With Siri in English, "roxo" arrived as "Who";
  "Purple" and "Red 50%" worked. So with Siri, say the English words.
- **Started without Siri** (tap, Back Tap, Action Button): Dictate Text uses its own language, so
  Portuguese works.
- "run" before the name helps Siri treat it as a shortcut.

## From the watch

The **Honda City** Connect IQ app (Forerunner 255) shows both lights, changes each zone's colour
and level, applies the presets and turns music mode on and off, through Garmin Connect on the
iPhone. It signs every request. How it works, and how to build and install it:
[garmin/README.md](../garmin/README.md).

## Updates over WiFi

The deck takes new firmware over WiFi, like the black deck: ArduinoOTA on port 3232, with
`OTA_PASSWORD` from `src/config.h` (its own random string, never `REMOTE_TOKEN`; updates are off
without it). The deck joins WiFi only with `REMOTE_TOKEN` set, so it needs both.

- **Once over USB**: `pio run -e ledpanel -t upload`. This build switches the deck to a flash
  layout with two app slots; the settings (touch calibration, looks, trip, the controller's bond)
  stay where they were.
- **Then from a PC on the iPhone hotspot**: `pio run -e ledpanel_ota -t upload`. It sends to the
  deck's hotspot address, `172.20.10.11`. Windows may move the PC back to the home WiFi on its
  own: check it is still on the hotspot before an upload.
- **On the screen**, in the current theme: `>UPDATING` with a 10-cell bar and the %, then
  `>REBOOTING`. Meanwhile the lights stay as they are, the lyrics and weather polls stop, and the
  touch, the controller and voice wait (`/say` answers busy). A failed or cut upload shows
  `>UPDATE FAILED` for about 3 s, then the screen that was showing; the running firmware stays.
- **Keep the deck powered for about a minute after an update.** New firmware proves itself once
  it has run a minute online with its update listener up (`[ota] update confirmed`); any reset
  before that, a crash or the ignition, boots the previous firmware. After that, three crashes in
  a row before it is marked good still go back (`[ota] rolled back`).

## Notes

- At home the deck may join home WiFi instead of the hotspot (the iPhone only shows its hotspot
  while the Personal Hotspot screen is open), and then the iPhone can't reach it: open that screen
  and restart the deck. It stays on the network it joined; in the car only the hotspot exists.
