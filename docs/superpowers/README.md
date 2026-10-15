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
| 2026-10-15 | `specs/2026-10-15-obd-gauges-design.md` | `plans/2026-10-15-obd-gauges.md` | implemented on the companion; on-car check pending |
| 2026-10-14 | `specs/2026-10-14-avatar-home-design.md` | `plans/2026-10-14-avatar-home.md` | implemented (2303503); revised to phone-driven (touch layer faulty, e089b4d) and Home shows her alone, centred (1473f97) |

Plans keep the machine-specific commands they were executed with (`.devtools\…`, `COM11`);
see the main README and
[CONTRIBUTING](../../CONTRIBUTING.md) for portable commands. These records were written with
an AI-assisted design workflow ([superpowers](https://github.com/obra/superpowers)) and are
kept as-is for history. Code review and cleanup: `../review/`.
