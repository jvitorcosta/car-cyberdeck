# Car lights by voice (Siri Shortcuts)

The LED panel on the old deck (`pio run -e ledpanel -t upload`) takes spoken commands from an
iPhone Shortcut over the hotspot. Siri turns your voice into text, the Shortcut sends it to the
deck, the deck changes both lights and answers with one line that Siri speaks back.

- Address: on the iPhone hotspot the deck is always `172.20.10.11`.
- `GET /say?text=<phrase>&k=<REMOTE_TOKEN>` changes the lights; the reply is e.g. `Purple, 70 percent`.
  When the change was for a light the deck has no link to, the reply says so:
  `Purple, 70 percent, outside not connected` (`inside not connected`, or `lights not connected`
  when neither is linked).
- `GET /state?k=<REMOTE_TOKEN>` answers the same line without changing anything.
- Both take an optional `&zone=inside|outside`: then only that light's page changes (or is read),
  and the reply is prefixed: `Inside: Purple, 70 percent`. Without it, both lights (the BOTH page).
- `GET /presets?k=<REMOTE_TOKEN>` lists the six presets, one line each, joined by newlines:
  `1 ROSE pink 40 pink 100` (slot, name, inside colour and level, outside colour and level). A
  colour can be two words: `3 GOJIRA light blue 40 light blue 100`. The list is fixed, so it never
  changes between calls.
- `GET /status?k=<REMOTE_TOKEN>` tells whether each light is linked and on, then the firmware's
  version: `inside=on outside=nolink fw=1.1.0+226.g41b2b7a`. Each light's word is `on`, `off` or
  `nolink`; a light the deck has no link to is always `nolink`, whatever it last showed. The same
  `FW` version types out under the boot's welcome lines.
- All of them reply in plain text (`text/plain`). A wrong key gets 403 "Wrong key" (on all of them); a phrase with
  nothing to do gets "Sorry, I didn't get a colour or a command".
- **Signed requests**: instead of `k=`, any of the four takes a signature, `t=<unix s>&n=<16 hex>&sig=<32 hex>`
  (the first 16 bytes of HMAC-SHA256 with `REMOTE_TOKEN` over `GET|<path>|<params>|<t>|<n>`, params sorted
  by name and percent-encoded; `src/util/wire/sigreq.h`). The Garmin app signs; Siri Shortcuts can't, so
  they keep `k=`. A signed request must be within 2 minutes of the deck's clock (else 403 "clock off")
  and is accepted once; before the deck's first SNTP answer it gets 503 "clock not set" (the `k=` still
  works).
- Siri sends no zone, so voice always changes both lights; spoken zone words are ignored. The page
  on the deck's screen is left as it is.

## On the deck

The deck boots by typing the welcome (`WELCOME TO CYBERDECK`, character by character in about half
a second), then shows the **main screen**, whatever page it last showed. The look is "Seegson"
(Alien: Isolation): pale green on black, one hue at three brightnesses, and red kept for problems
only (`-- NO LINK`, `>OFFLINE`, the `NO LINK` messages, `>WRONG KEY`; a deeper red at night). Every
screen has faint scanlines and ALL CAPS bracket labels. To read at a glance in a moving car, every
colour clears a contrast on black of at least 4.5:1 by day (the dimmest green too) and 3:1 at night
(WCAG; `src/util/yellow/termpal.h` lists each one's figure), and the darker greens carry no blue, which
the panel would turn teal.

- **Main screen** (a dashboard, top to bottom; it puts first what the car's panel and CarPlay don't
  show: the trip and the lights):
  - the header `CYBERDECK // HONDA CITY TOURING 2022`;
  - the clock big with the date under it (`FRI 23 OCT`; `--:--` and `--- -- ---` until WiFi has
    synced the clock), and on the right the outside temperature big (`32°C`, `--°C` before the first
    reading) and the sky in words (`PARTLY CLOUDY`); under them, on the right, `HI 32 LO 26  MANAUS`
    (dim; `HI 32 LO 26` if a city were ever too long for the row). Where the car is comes from
    the hotspot's public IP (ip-api.com, approximate), the weather from Open-Meteo, every 15 min.
    A failed fetch keeps the last reading; `>OFFLINE` shows only when no reading ever came;
  - the trip: `TRIP`, its time big (`0:32`), and on the right the distance (`12.4 KM`, or a dim
    `-- KM` while no speed source feeds it);
  - both lights on one row, split by a thin rule: `IN`, a chip of its colour, the colour name and
    level (`ROSE 40%`) on the left, `OUT` the same on the right; `OFF` (dim) for a light that is off,
    `-- NO LINK` (red) when the deck has no link to it (never its last colour);
  - the prompt line: `>` and a steady block cursor; a message while one shows (see Messages; the
    cursor blinks then), and music mode's line while it is on (see "Music mode: song colours" below);
  - at the bottom, three big buttons (44 px tall): `[LIGHTS]` opens the Lights page, `[LYRICS]`
    the black deck's lyrics, `MUSIC MODE` turns music mode on or off, with an inverted `ON` or
    `OFF` tag under its label. LYRICS is bright with a small inverted `LIVE` tag at its right edge
    while the black deck plays a song with synced lyrics, and dim (no tag) otherwise: a tap then
    says why. The song itself is on the Lyrics page only.
- There is no Weather page any more: its data is on the main screen. The presets are on the Lights
  page's PRESETS tab only (and voice, the watch, `/presets`).
- **Lights**: `< BACK` (the same boxed button as on Lyrics) returns to the main screen. Then
  Pip-Boy style tabs for BOTH / INSIDE / OUTSIDE / PRESETS (in the small face: four tabs fit beside
  `< BACK` only at 56 px each, too narrow for the body face's `PRESETS`), and the status line. On
  BOTH, INSIDE and OUTSIDE: the 2x4 colour boxes (a tall colour chip and the name; the chosen one is
  the inverted box) and a 10-cell level bar with `-` / `+` (its label is the %, `MIX` or `OFF`).
  Tap a tile to set the colour and turn on; double-tap the lit one to turn off. On **PRESETS**: the
  six presets as big buttons, two rows of three, each its name over two colour chips (inside,
  outside); a tap applies it (`>ROSE APPLIED`) and the tab stays. The page last shown (PRESETS too)
  is kept over a reboot, and `[LIGHTS]` opens on it. The Lyrics page's song row is in capitals
  like the rest of the chrome; the lyrics keep their case. With nothing to sing along it shows,
  big and dim, the same words as LYRICS's messages: `>NOTHING PLAYING`, `>NO SYNCED LYRICS`,
  `>LYRICS: NO SIGNAL` (or `>WRONG KEY` in red).
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
  from any source), `>INSIDE: NO LINK`, `>OUTSIDE: LINKED` (a light's link changed), why LYRICS
  won't open (`>NOTHING PLAYING`, `>NO SYNCED LYRICS`, `>LYRICS: NO SIGNAL`, `>WRONG KEY`), and at
  boot the trip before (`>LAST TRIP 12.4 KM 0:47`, or `>LAST TRIP 0:47` without a distance; 6 s).
  The cursor moves to its end and blinks while it shows. Then the prompt line is back to `>` (on
  Lights: the status line).
- **Taps**: a pressed button is the solid bar with a ring cut into it, in the theme's own colours,
  so it differs from an idle box and from the selected (plain) bar on every theme, day and night. After a screen or
  page change the deck waits for the finger to lift and a quarter second to pass before it takes
  another touch, so a double tap on `[LIGHTS]` can't press `-` or PURPURA on the next screen.
- **Xbox controller** (BLE): D-pad moves the focus (an inverted bar on the main screen's three
  buttons, left / right between them; a ring in the theme's text colour on Lights), **A** presses,
  **B** is Back (to the main screen; there it does nothing). On Lights only: **X** next page (BOTH,
  INSIDE, OUTSIDE, PRESETS, round again); on BOTH / INSIDE / OUTSIDE **Y** this page's lights
  on/off, **LB / RB** `-` / `+` (on PRESETS they do nothing; the D-pad moves over the six presets,
  A applies). B used to switch to the BOTH page; use X for that now. The focus shows only while the controller
  is connected: without it the main screen's three buttons look alike, so the clock and the
  lights lead.

## Presets

Six fixed presets replace the old saved favourites. They are in the firmware, so nothing is saved
on the deck and there is no hold-to-save. Each sets **both** lights, both on, one colour at 40 %
inside and 100 % outside:

| # | Name | Inside | Outside |
|---|---|---|---|
| 1 | ROSE | pink 40 % | pink 100 % |
| 2 | FIRE | red 40 % | red 100 % |
| 3 | GOJIRA (ゴジラ on screen) | light blue 40 % | light blue 100 % |
| 4 | PURPURA | purple 40 % | purple 100 % |
| 5 | CIVIC | green 40 % | green 100 % |
| 6 | OCEAN | blue 40 % | blue 100 % |

GOJIRA (2026-10-26, in CALM's place; FOREST became CIVIC the same day) is Godzilla's atomic
breath. Its name shows as **ゴジラ** on the deck's PRESETS tab and in `>ゴジラ APPLIED`: VT323 has no
kana, so the three glyphs are 16 px bitmaps from GNU Unifont (the black deck's `data/cjk16.bin`),
made by `tools/gen_gojira_glyphs.py` into `src/yellow/fonts/gojira16.h` and drawn by `src/yellow/ui/gojiraname`.
On the wire (`/presets`), in the logs and in Siri's replies it is `GOJIRA`; the watch shows the
same glyphs as a bitmap.

**Light blue** is GOJIRA's colour and a preset's only: an icy electric cyan-blue, `#0082FF` on the
deck's screen, LED mix `0,130,255` (B, the owner's pick on 2026-10-26). It is not on the Lights page's 2x4 grid, and song
colours never snap to it (music mode keeps to the grid's eight). Voice can set it ("light blue",
"azul claro", "cyan"), `/state` and `/say` reply `Light blue, 40 percent`, and the watch shows it.
With light blue on the inside light the terminal takes the blue theme. The main screen's lights
row calls it `CYAN` (`LIGHT BLUE 100%` doesn't fit a half); Siri's replies and the watch keep
"Light blue".

- **Apply**: tap a preset on the Lights page's PRESETS tab, or Xbox A on it. That is all: no hold.
- **Voice**: "rose", "fire", "gojira" (or "godzilla"), "purpura", "civic" or "ocean", or "preset
  one" ... "preset six" (also
  `favourite`/`favorite`, and digits: "preset 2"). It applies to both lights, whatever zone was
  asked, and ignores the rest of the phrase ("rose 50" is just ROSE). The reply is e.g.
  `Rose: inside Pink 40 percent, outside Pink 100 percent`. Siri may hear "purpura" as "purple",
  which is the colour; say "preset four" for that one. "calm" and "forest" do nothing any more.
- **Watch**: the Honda City app has a fourth page, **PRESETS**: each preset as a bar, inside on the
  left and outside on the right, each half filled with its colour to its level (all six, in shorter
  bars than four would get). START there opens
  the same bars as a list; START on one applies it. "No presets yet" shows if `/presets` didn't
  answer.
- Presets with different looks inside and outside make the BOTH page show `MIX` (see "BOTH while
  the lights differ" above); the main screen shows each light's colour and level.

## Music mode: song colours

`MUSIC MODE` on the main screen (a tap, or Xbox A on it) turns it on or off; the tag under it says
`ON` or `OFF`, and `>MUSIC ON` / `>MUSIC OFF` shows on the prompt line. It is never kept over a
reboot. There is no mic: the black deck picks the current song's colour (one of the 8, see "Which
colour" below; the same day and night, only the brightness cap follows the clock) and sends it over
WiFi, and the deck does the rest.

- **Colour**: both lights take the song's colour, with that colour's tuned LED mix, the same as
  tapping its tile. It stays steady through the song. On a new song it fades to the next colour over about
  3 s (inside in 6 steps, outside in 3: the outside controller takes about one command a second).
  The black deck waits for the new song's cover (up to 8 s) before it sends, so a song changes the
  lights once.
- **Brightness**: each light at its own level as you left it, times the time-of-day cap of the
  palette (day 100 %, evening 70 %, late night 45 %), but never under 40 %; a light you had at 40 %
  or less keeps its level. At 18:00 and 22:00 a playing song's lights fade to the new level once; the colour
  stays.
- **Off lights**: an outside light you left off stays off; the inside light is switched on (that is
  the point of the mode).
- **Screen**: the terminal theme follows the song's colour, as it follows the inside light. The
  lights row shows what the lights show (`IN [chip] PINK 45%`). The prompt line shows `>MUSIC` and
  a chip of the colour with its name (`>MUSIC [chip] PINK`); `>MUSIC: WAITING` until the first
  palette comes, and `>MUSIC: NO SIGNAL` (normal colour, not red) when no palette came for 15 s
  (the lights keep the last song's colour).
- **Ending it**: any manual change from anywhere (a colour, preset, level, on/off on the touch
  screen, the Xbox controller, Siri, the watch) ends the mode, then that change applies to your own
  looks. Turning the mode off brings back the looks from before it started. Your looks are never
  saved over while it runs, so a reboot also brings them back.
- **Watch and `/state`**: while the mode drives the lights they report what the lights show.
- **Black deck**: the yellow deck asks `GET /music?on=1` (signed, like its `/state` polls: it never
  sends the key) every 10 s while the mode is on, `on=0` when it ends; the black deck stops by itself
  30 s after the last ask. `/music` also takes `k=<token>` for a test by hand. Until its clock is set
  the yellow deck asks nothing (it waits rather than send the key). The palettes come over UDP
  (port 4210) signed: each packet carries the black deck's boot id, a counter that rises with every
  packet and an HMAC tag with `REMOTE_TOKEN` (`src/util/wire/musicwire.h`). The `/music?on=1` reply
  proves the boot id for that request's nonce (`{"on":1,"boot":..,"proof":".."}`); the yellow deck
  drops a packet with a bad tag, from another address, from any other boot, or with a counter not
  above the last one, so a spoofed or replayed packet can't set the lights. Both decks need this firmware: the old
  unsigned packet is refused.
  `GET /music/debug?k=<token>` on the black deck shows the palette it last sent, its cap and the
  socket (`&on=1` also keeps it alive).

### Which colour

The black deck decides (`util/songmood`), strongest signal first; text matches ignore case and
accents, and titles and albums also match without a suffix such as ` - Remastered`,
` - Single Version`, ` (feat. ...)` or ` (with ...)`:

1. **Your picks** always win: a song (title and artist), an album or an artist, each to a colour
   (a song's pick beats its album's, which beats its artist's). They live in
   `src/util/black/songpicks.inc`. To add one, add a line there and rebuild the black deck:
   `{"Artist", "Song title", nullptr, RED},` for a song, `{"Artist", nullptr, "Album", PINK},` for a
   whole album, `{"Artist", nullptr, nullptr, BLUE},` for every song of an artist (Budah is purple).
   Colours: `RED`, `ORANGE`, `PINK`, `PURPLE`, `BLUE`, `GREEN`, `WHITE`, `YELLOW`.
2. **A colour word in the song's title** (whole words): purple/púrpura/roxo/violeta,
   red/vermelho/vermelha, blue/azul, pink/rosa, green/verde, yellow/amarelo/gold/dourado/ouro,
   orange/laranja, white/branco. "Púrpura" is purple, Djavan's "Azul" blue; "Goldfrapp" is no gold
   (whole words only), and artists' names don't count.
3. **Your playlist's colour tags**: write them anywhere in a playlist's description on Spotify.
   `#white` makes every song in it white; `#red #pink #purple` makes those its colours (the cover
   chooses among them, as for a mood below; a grey cover takes the song's genre colour inside them,
   else the first tag). Tags: #red/#vermelho/#vermelha, #orange/#laranja, #pink/#rosa,
   #purple/#roxo/#roxa/#púrpura/#violeta, #blue/#azul, #green/#verde, #white/#branco/#branca,
   #yellow/#amarelo/#amarela/#gold/#dourado (any case, accents or not; `#redemption` is no tag).
   The deck reads the description when it looks the playlist up, so new tags apply after you play
   the playlist from another context first, or after a restart. Liked Songs has no description.
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
   black-and-white cover too): under a playlist's tags or mood, the first colour of the song's genre list
   that the playlist allows (a grey pop song in a neon playlist is pink; the log says
   `playlist mood neon, genre pop`, or `tags, genre intimate`); with no overlap or no genre, or when the genre gave the mood,
   the mood's first colour. A white-ish cover colour counts as white only where white is allowed.

The time of day changes only the brightness, never the colour. The black deck's serial log has one
line per song with what it knew and which rule decided:
`[colour] "Popular" | The Weeknd | Popular | "blinded by the lights 🌃" | genre R&B | cover FF0800 FF9000 | playlist mood neon | -> Red cap 100`.

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

While the **INSIDE** light is on and linked, the whole terminal (main screen, Lights, Lyrics; not the
boot typing or the touch calibration) takes that light's colour: the same three brightnesses in its
hue, by day and darker at night (`termpal::rgb(role, night, theme)`, every colour's contrast in
`src/util/yellow/termpal.h`). Blue and purple are lighter shades so the text reads; green by day is the usual
green; white is a pale grey-white by day and the usual **amber at night** (no white glare). Problems
stay red, except on the red, orange and pink themes, where they show in **white** (a dimmer white at
night) so they stand apart from the text. The colour tiles, chips and preset chips keep their real
colours. Light blue, which has no theme of its own, takes blue's. With the inside light off or out
of reach the screen goes back to the green / amber. To
see a theme without the car's lights, build with `-D THEME_TEST=<0..7>` (red, orange, pink, purple,
blue, green, white, yellow) the same way as `NIGHT_TEST` above, then clear it.

## What it understands

English or Portuguese, any order, other words ignored (`src/util/yellow/ledvoice.h`):

| Kind | Words |
|---|---|
| Colours | red/vermelho, orange/laranja, pink/rosa, purple/roxo/lilás, blue/azul, green/verde, white/branco, yellow/amarelo (also Siri's "read" = red, "blew" = blue); light blue: "light blue" (the two words together), azul claro/clara, cyan/ciano |
| Power | on, off / liga, desliga, acende, apaga |
| Brighter / dimmer (10 % steps) | brighter, up, more / mais, aumenta, forte; dimmer, down, less / menos, diminui, fraco |
| Level | a number (`50`, `50%`, rounded to 10), max/máximo, min/mínimo |
| Preset | rose, fire, gojira/godzilla, purpura, civic, ocean; or preset/favourite/favorite + 1-6, as a word or digit ("preset two", "preset 2") |

A colour also turns the lights on. "light" only counts right before "blue" ("light off" is just
off), so "turn the light blue" means light blue, and "lights blue" means blue.

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
- **PRESETS page** (fourth page): the deck's six presets as bars, the name over each (a size smaller
  than with four, so all six fit between the pills). The left
  half is the inside light, the right half the outside one, each filled from its left edge to its
  level (the rest dark grey; a light that's off says OFF). START opens the same bars as a list
  ("No presets yet" if the deck didn't answer): the focused bar is taller, outlined in white and
  shows both levels. START on it applies it, buzzes, says APPLIED for a second and returns to the
  pages; BACK leaves without applying.
- **Purple** is `AA00FF` on the watch: the deck's `#8C00FF` as the watch's 64 colours show it.
- **Light blue** (GOJIRA's) is `00AAFF` on the watch, apart from OCEAN's blue and from green, with
  black text on it. The zone lists keep the deck's eight grid colours (a list opened on a light-blue
  light starts on Blue); light blue comes from the preset. GOJIRA's bar shows its name as **ゴジラ**,
  a small bitmap of the deck's Unifont glyphs (the watch's fonts have no kana).
- **Not connected**: when a light has no link, its page's bottom line says "Not connected" (on
  BOTH: "Inside not connected", "Outside not connected", or "Not connected" for both) in orange in
  place of the "START: change" hint. The page still shows that light's last known colour. The app
  reads this from `/status` when it opens.
- **Glance**: a card for the watch face's glance loop with both lights' dots and levels, as of
  the last time the app ran (a glance can't make web requests). START on it opens the app.
- Requests go one at a time from a short queue; every reply is read back into the screen, so it
  shows what the deck reports. Errors: "No answer", "Phone not connected", "Wrong key", "Clock off: sync
  watch" (the watch's clock is more than 2 minutes off the deck's: the deck answers 403 "clock off"),
  "Deck has no clock" (no SNTP answer yet).
- Key: copy `source/Secrets.mc.example` to `source/Secrets.mc` (git-ignored) with REMOTE_TOKEN. The
  app signs each request with it (`source/Sign.mc`, `Toybox.Cryptography` HMAC-SHA256, the watch's
  clock) and never sends it. `monkeyc ... --unit-test` and `monkeydo <prg> fr255 /t` in the simulator
  run its known-answer tests (`source/SignTest.mc`, the same vectors as `test/test_sigreq`).
- Build: Connect IQ SDK + a developer key, `monkeyc -d fr255 -f monkey.jungle -o bin/CarLights.prg
  -y <key> -l 2` (the manifest also lists fr255m, fr255s, fr255sm).
- Install: copy `bin/CarLights.prg` to the watch's `GARMIN/APPS` over USB (if Windows asks about
  the file already there, keep the new one), then unplug. To have it in the glance loop, add
  Honda City in the watch's glance settings.

## Updates over WiFi

The panel takes new firmware over WiFi, like the black deck (ArduinoOTA on port 3232, the password
is `OTA_PASSWORD` in `src/config.h`, its own random string apart from `REMOTE_TOKEN`; off without
one). The panel joins WiFi only with `REMOTE_TOKEN` set, so it needs both.

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
- **Rollback**: keep the panel powered for about a minute after an update. New firmware proves
  itself once it has run a minute, is online and listens for updates (`[ota] update confirmed`);
  any reset before that, a crash or the ignition, boots the previous firmware (the bootloader's
  rollback). After that, three crashes in a row before it is marked good still go back
  (`[ota] rolled back` in the serial log).

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
