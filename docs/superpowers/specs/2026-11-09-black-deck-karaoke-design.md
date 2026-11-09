# Karaoke on the black deck

Date: 2026-11-09. Status: design, approved by João; not built.

## Why

The yellow deck (ESP32 CYD, no PSRAM, ~25-37 KB of heap with the PS3 controller on Bluedroid's dual
mode) shows the karaoke today. To do it, it asks the black deck (ESP32-S3) for `/state`, a ~2 KB
JSON with the current and next line, every second, plus extra polls when a line changes. That
request is the yellow deck's heaviest WiFi and heap load. The radio it uses is shared with the two
lights and the PS3 controller, and the heap dips it causes were behind the restarts fixed in PR #54.

The black deck already has the lyrics: it fetches them from LRCLIB, times them against the song and
shows the current line in its dialogue box. So the karaoke moves to the black deck's own screen, and
the yellow deck becomes the switch for it, the way it already is for music mode.

Success, as João put it: big lines synced with the song, a karaoke-style sweep on the line being
sung, the black deck's walking Pokémon stepping along that line, and the next line waiting
underneath for its turn. The yellow deck stops carrying the lyrics.

## Goals

- A full-screen karaoke view on the black deck, in the black deck's own style (GBA-style boxes,
  its theme), chosen from the mockups as option B:
  - the song name, artist and progress bar at the top;
  - a large dialogue box holding the line just sung (small, at the top), the current line (large)
    and the next line (small, underneath);
  - the current line starts in a light tone and a dark tone sweeps across it from left to right as
    it is sung;
  - the walking Pokémon (the same one the deck already walks for the song) steps along the top of
    the current line, at the sweep's leading edge;
  - when the line's time is up, the next line becomes the current one and the Pokémon starts on it.
- KARAOKE on the yellow deck is an on/off switch like MUSIC MODE: lit while on, off only by a tap.
- While karaoke is on, each song decides the black deck's screen:
  - a song with synced lyrics shows the karaoke view as soon as its lyrics are found;
  - a song without synced lyrics shows that in the deck's Pokémon-style message, then returns to the
    normal screen (the walking Pokémon) 30 s after the deck has decided there are none;
  - a later song with synced lyrics brings the karaoke view back by itself, while the switch is on.
- The yellow deck stops polling `/state` for lyrics, and its own KARAOKE page goes away.

## Non-goals

- Word-level timing. LRCLIB gives one time per line (`util/black/lyricbuf`, `lrcstream`): the sweep
  is paced evenly from the line's start to the next line's start. A held note or a pause makes it run
  a little ahead or behind; that is accepted.
- A new lyrics source, or changes to how the black deck finds lyrics (`black/lyrics/lrclib`).
- A Pokémon pixel font. The black deck draws text with TFT_eSPI's font 2 plus its accent, CJK and
  emoji overlays (`ui/textdraw`); the karaoke view uses those, scaled. "Pokémon style" here means
  the deck's boxes, colours and sprite, not a new typeface.
- Touch or controller input on the black deck. The switch lives on the yellow deck (touch, the PS3
  controller's focus and cross, and anything else that can press its items).

## Anti-goals (a failure even if it works)

- The black deck stuck in karaoke with nothing able to turn it off.
- A screen that says "no lyrics" for a long time while driving.
- The yellow deck's heap or radio load going up instead of down.
- Lines that jump or flicker as they change, or a Pokémon that teleports.
- Breaking music mode, the phone page or the watch, which share the same HTTP routes.

## How it behaves

### The switch

- The yellow deck holds the switch, like music mode. While it is on, the yellow deck sends a signed
  request to the black deck at once and then every 10 s (the music keep-alive's pace, `MUSIC_KEEP_MS`).
- The black deck keeps karaoke on while those requests keep coming. If none arrives for 30 s (the
  yellow deck switched off, out of WiFi), it turns karaoke off and stays on its normal screen.
- A tap to turn it off sends an "off" request once; the black deck leaves karaoke at once.
- The voice command "karaoke" / "karaoke off" switches it, like "music" / "music off"
  (`util/yellow/ledvoice`). (Design call, not João's.)
- Karaoke and music mode are independent: both can be on together (the lights follow the song's
  colours while the black deck shows the lyrics).
- The switch is not saved across a reboot of either deck. (Builder's call to revisit if the
  car test shows it should be.)

### Which screen the black deck shows while karaoke is on

Per song, from the black deck's lyric status (`util/wire/lyricstatus`):

| Status | Screen |
|---|---|
| Searching, Retrying | the karaoke view with the deck's existing searching message in the box (`ui/lyricmsg`); this time never counts toward the 30 s |
| Synced, before the first line | the karaoke view, the first line shown dim with nothing swept yet (an intro is waiting, not missing) |
| Synced | the karaoke view, sweeping |
| Plain, Instrumental, None | the Pokémon-style "no lyrics" message for 30 s, then the normal screen until the next song |

- "Plain" (lyrics without timing) counts as no lyrics: there is nothing to sweep.
- Pausing Spotify keeps whatever is on screen; on the karaoke view the sweep and the Pokémon stop where
  they are.
- The black deck's other screens keep their priority over karaoke as they have over the normal
  screen: the OTA screen, the offline and waiting status scenes, and the camera band (which shrinks
  the view below it, as it does the normal screen).

### The karaoke view

- The view replaces the normal now-playing screen while it shows. The black deck is 320x240.
- The current line is drawn large (font 2 at 2x, `textdraw::drawRunScaled`) and wraps to two rows when
  it doesn't fit; the sweep runs along the first row, then the second. The previous and next lines are
  drawn small and cut with "..." when too long.
- The sweep's position comes from the track position the deck already interpolates (`interp`), the
  current line's start and the next line's start: `x = width * (pos - start) / (nextStart - start)`,
  capped at the line's end. For the last line, the builder picks a sensible end (a few seconds, or the
  song's end).
- The Pokémon walks with its existing frames and frame timing (`images/walksprite`, `util/black/walkanim`)
  but its x follows the sweep instead of the song's progress. It stands on the current line's top
  edge, above the text.
- When a line ends and the next becomes current, the Pokémon hops back to the left edge: a quick
  arc-shaped leap over the box, a few frames long, landing where the new sweep starts. It never
  teleports and never walks backwards. (João's choice.)
- Japanese and Chinese lines draw with the deck's CJK font, as the dialogue box does now.
- A current line that wraps to two rows: the Pokémon stays on the first row's top edge and its x
  follows the sweep's progress through the whole line (both rows mapped onto the row's width), so it
  never drops onto text. (Design call, not João's.)
- A timed blank line (LRCLIB marks instrumental breaks with empty lines, and `lyricbuf` keeps them):
  the line just sung stays as the current line, fully swept, with the Pokémon resting at its end; the
  next line waits underneath as usual. The blank line is never shown as the "line just sung".
  (Design call, not João's.)

## What changes on each deck

### Black deck (`src/black`)

- A karaoke on/off route on the existing HTTP remote (`net/remote`), signed and checked the same way
  as `/music` (`reqauth`), with the 30 s keep-alive.
- Access to the previous line from the UI (`core/shared` exposes the current and next today; the
  arena holds every line, so a previous-line accessor is small).
- The karaoke view and the screen choice above, in `main.cpp`'s mode logic and a new UI module next to
  `ui/screen_now`.
- Host tests for the pure parts: the screen choice per status and the 30 s timer, and the sweep's x.

### Yellow deck (`src/yellow/ledpanel.cpp`)

- KARAOKE becomes a switch item like MUSIC MODE, lit while on. The KARAOKE page, its drawing and its
  Japanese font (`data/jp16.bin`, ~121 KB of flash; `ui/jpfont`, `ui/lyricrun`, `util/yellow/karaoke`)
  go, with their host tests.
- The "karaoke" task (`lyricsTask`) stops fetching lyrics. It still does what else it owns: music
  mode's keep-alive and boot proof (`musicKeepAlive`, `musicBoot`, `musicPeer`), finding the black
  deck (fixed hotspot address, then mDNS, `deckaddr`), and now the karaoke keep-alive. It can likely
  shrink its stack; the `[mem]` line will say.
- The Menu's now-playing band (track, artist and progress bar, `drawNowPlaying`) goes: the black
  deck's own screen shows the song. With it, the yellow deck no longer requests or parses `/state`
  at all (`stateparse`, `pollpace`, `lyricsWant` and their host tests go too, unless something else
  still uses them), and the Japanese font goes with nothing left drawing Japanese. (João's choice.)
- The KARAOKE item is not lit or dimmed by whether the song has lyrics any more (there is no poll to
  know): it shows the switch's state only.

### Docs

`docs/yellow-deck.md` (the karaoke sections, the Menu, Memory), `README.md` (the yellow deck's row),
`docs/ARCHITECTURE.md` (the task table and wire table), the black deck's own docs for the new view and
route, and the CHANGELOG.

## Constraints

- Public repo: no secrets, addresses, SSIDs or personal data in code, docs or logs.
- Conventional commits; no merge without João.
- The yellow deck flashes over USB only; the black deck by USB or `s3_ota`. Never touch the black
  deck's port without asking.
- Builds stay clean with `-Werror`; host tests in `tools/run_tests.ps1` and pytest in `tools` pass.
- The yellow deck's heap must not go down: measure `[mem]` before and after.

## Left to the builder

- The exact layout, sizes and colours of the view within the mockup's arrangement.
- The route's name and parameters, and whether karaoke rides on the music keep-alive or has its own
  request.
- How the last line of a song ends its sweep.
- The hop's exact frames and height.
- What fills the Menu's space where the now-playing band was.
