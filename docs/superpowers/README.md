# Design records

Specs (what and why) and plans (step-by-step tasks) from the structured design workflow, in
date order. Later specs extend earlier ones; the README "Design & performance history"
summarised the outcome of each; that
summary is now [`../HISTORY.md`](../HISTORY.md).

| Date | Spec | Plan | Status |
|---|---|---|---|
| 2026-10-04 | `specs/2026-10-04-pokedeck-esp32-spotify-design.md` | `plans/2026-10-04-spotify-pokemon-deck.md` | implemented; walker superseded, touch not built |
| 2026-10-06 | `specs/2026-10-06-merged-status-panel-pmd-walker-design.md` | `plans/2026-10-06-gen3-battle-ui-pmd-walker.md` (Parts 1–4) | implemented |
| 2026-10-06 | `specs/2026-10-06-perf-network-task-design.md` | same plan, Part 2 | implemented |
| 2026-10-06 | `specs/2026-10-06-memory-resilience-design.md` | same plan, Part 3 | implemented |
| 2026-10-07 | `specs/2026-10-07-cjk-text-design.md` | same plan, Part 4 | implemented |
| 2026-10-07 | `specs/2026-10-07-lyrics-reliability-design.md` | `plans/2026-10-07-lyrics-reliability.md` | implemented (f45da1a) |
| 2026-10-08 | `specs/2026-10-08-genre-badge-design.md` | `plans/2026-10-08-genre-badge.md` | implemented (3ccf495, aca6c90, 6836503) |
| 2026-10-08 | `specs/2026-10-08-night-mode-design.md` | `plans/2026-10-08-night-mode.md` | implemented (8973b0e, 7590a4e, ecc2177) |
| 2026-10-09 | `specs/2026-10-09-boot-scene-design.md`, `-v2-`, `-v3-design.md` | `plans/2026-10-09-boot-scene*.md` | implemented as v3 (19b19a1); v1 and v2 superseded |
| 2026-10-09 | `specs/2026-10-09-s3-port-design.md` | `plans/2026-10-09-s3-port.md` | implemented (8ddb104, 25b64a1) |
| 2026-10-13 | `specs/2026-10-13-companion-screen-design.md` | `plans/2026-10-13-companion-screen.md` | v1 cut to lyrics only (2eaf363); later extended by the avatar home (trip time on its Trip screen, buttons on the phone page); the clock and weather strip and the retro map not built |
| 2026-10-14 | `specs/2026-10-14-avatar-design.md` | `plans/2026-10-14-avatar.md` | step 1 superseded: moved from the S3 to the companion during implementation (spec revision, ac13e2c); converter, file format, lines and singing kept |
| 2026-10-14 | `specs/2026-10-14-avatar-home-design.md` | `plans/2026-10-14-avatar-home.md` | implemented (2303503); revised to phone-driven (touch layer faulty, e089b4d) and Home shows her alone, centred (1473f97); retired with the companion screen on 2026-10-28 |
| 2026-10-19 | `specs/2026-10-19-random-intro-design.md` | `plans/2026-10-19-random-intro.md` | implemented (49926e0); a fourth opening, the second video, came later (#17) |
| 2026-10-22 | `specs/2026-10-22-emoji-design.md` | `plans/2026-10-22-emoji.md` | implemented (#14); joined emoji (#15) and frequency tier 11 (#16) on top |
| 2026-10-22 | — | `plans/2026-10-22-main-review-bugs.md` | implemented: the 2026-10-21 review's findings still real on main (bb4774c, aba78ba, 101666e and others) |
| 2026-10-22 | `specs/2026-10-22-shadow-update-screen-design.md` | `plans/2026-10-22-shadow-update-stage1.md` | implemented, with Green Hill Zone in place of Star Light Zone (a29bc0d, the spec's own revision) and Shadow on the rail (b7cb60c) |
| 2026-10-23 | `specs/2026-10-23-yellow-deck-menu-design.md` | `plans/2026-10-23-yellow-deck-menu.md` | menu and night mode implemented (#18); the four saved favourites superseded by six fixed presets (#18, #20); the Car, Heart and Music pages not built (music mode came as song colours) |
| 2026-10-23 | — | `plans/2026-10-23-presets-restyle.md` | implemented (#18, then six presets and the PRESETS tab in #20) |
| 2026-10-24 | `specs/2026-10-24-music-mode-design.md` | `plans/2026-10-24-music-mode.md` | superseded by song colours (#20, #23): no microphone, no beat pulse; the voice word, the watch item and `/status music=` came in wave 4 (2026-10-28) |

Plans keep the machine-specific commands they were executed with (`.devtools\…`, `COM11`);
see the main README and
[CONTRIBUTING](../../CONTRIBUTING.md) for portable commands. These records were written with
an AI-assisted design workflow ([superpowers](https://github.com/obra/superpowers)) and are
kept as-is for history. Code review and cleanup: `../review/`.

## Paths before 2026-10-28

The records above name the paths of their day; they are kept as written. On 2026-10-28 the code
moved into one folder per deck (`git log --follow` and blame see through the moves):

| Before | Since 2026-10-28 |
|---|---|
| `src/main.cpp`, `src/{audio,core,genre,images,lyrics,pokemon,spotify}/` | `src/black/` (same names), but `core/embed.h` → `src/common/core/` |
| `src/ui/*` | `src/black/ui/*`; `termfont`, `jpfont`, `lyricrun`, `gojiraname`, `termlayout`, `presslook` → `src/yellow/ui/`; `backlight` → `src/common/ui/` |
| `src/net/*` | `src/common/net/*`; `remote.{h,cpp}` and `icon.inc` → `src/black/net/` |
| `src/board/board.h`, `src/board/s3/` | `src/common/board/board.h`, `src/black/board/s3/` |
| `src/ledpanel.cpp`, `src/fonts/` | `src/yellow/ledpanel.cpp`, `src/yellow/fonts/` |
| `src/companion/touch.{h,cpp}` | `src/yellow/touch.{h,cpp}` |
| `src/{hwcheck,hwcheck_s3,blescan,bletest,xboxtest,ps3test}.cpp` | `src/diag/` |
| `src/util/<module>` | `src/util/<group>/<module>`, groups below |
| `src/companion/`, `src/board/cyd/`, env `cyd_companion`, `util/{avatarbin,avatarlines,avatarscene,avatarsing,follow,lyricclock,menunav,phonepress,press}`, `tools/{gen_avatar,avatar_clip}.py` | removed (the companion screen; in git history before 2026-10-28) |
| config `COMPANION_DECK`, flag `CARDECK_COMPANION` | `BLACK_DECK_ADDR` (the old name still works), `YELLOW_DECK` |
| `docs/leds-voice.md` | `docs/yellow-deck.md` |
| the LED controllers' addresses in `src/ledpanel.cpp` | `LED_INSIDE_MAC`, `LED_OUTSIDE_MAC` in `src/config.h` |
| the swatch `LIGHT_BLUE` ("Light blue", "light blue" on the wire) | `CYAN` ("Cyan", "cyan") |

Util groups:
- `wire/`: hmac256, lyricstatus, musicwire, sigreq, songpalette, statejson, stateparse
- `lights/`: hue, ledlook, musiclights, tripclock
- `common/`: bootguard, daynight, hotspotip, jsonflat, jsonpath, netdiag, netup, otaview, readguard,
  remotegate, rgb565, roam, text, version
- `black/`: animdata, artmap, backdrop, bios, bootanim, carbin, ctxcache, dexset, genre (+
  genre_data.inc), genrebadge, glyphrun, howl, idlesprite, idlewait, interp, introclock, intropick,
  linkstatus, loudness, lrcstream, lyricbuf, mediapack, netplan, playerjson, qrfit, quietboot, ridersprite,
  songmood (+ songpicks.inc), termblend, walkanim, walkrect
- `yellow/`: bothlook, dashtext, deckaddr, deckmenu, elkcmd, geoip, httphead, karaoke, ledpresets,
  ledvoice, lightstatus, otapage, pollpace, tapguard, termpal, touchcal, touchguard, touchmap,
  tripmeter, weather
