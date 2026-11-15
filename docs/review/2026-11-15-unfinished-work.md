# Unfinished work audit (2026-11-15)

What was dropped, left half-done or switched off, across `main` (at #64), every local and remote
branch, PRs #1–#64 and the local worktrees. Read-only: nothing was deleted, merged or flashed.
Worktrees are named by folder (siblings of the main checkout, or under `.claude/worktrees/`).

How "merged" was checked: the repo squash-merges, so a branch counts as merged when its head is
the head of a merged PR (true for every merged PR's branch), or when its content is already on
`main`.

## Ranked findings

| # | What | Where | How far it got | Suggestion |
|---|---|---|---|---|
| 1 | **Controller effects off** (`LED_EFFECTS 0`): the #47 welcome and the #58 IR JUMP/FADE keys send nothing | `main`, `src/util/yellow/elkcmd.h`; `env:ledpanel_probe` still builds with `-D LED_EFFECTS=1` | Switched off in #64 for the bench test; open PR #63 (debug notes) lists the guessed effect numbers as a likely cause of the dark rear footwells | **Finish**: find the real mode numbers with `ledpanel_probe`, then turn the flag back on. The yellow LEDs+PS3 work in progress now belongs to another worker, so wait for it |
| 2 | **Voice agent** (Zero Two): agent, evals, mic diag firmwares | `feat/voice-agent` (36 commits, 136 behind `main`), worktree `spotify-pokemon-deck-voice` | Parts 1–2 built and host-tested; mic too weak in the shell; live steps owed (BACKLOG "Parked: voice agent") | **Finish** when the new deck and an external I2S mic arrive. Rebase first: it predates #37–#64. BACKLOG still calls the branch "local"; it is on origin now |
| 3 | **Varied Japanese greeting** at the Honda opening (10 greetings, never last boot's, time-of-day aware) | `claude/intro-greetings-8b8b67` (1 commit, on origin, no PR) | Built with a host test (`util/black/greetpick`); never opened as a PR; `greetpick` is not on `main` | **Finish or drop** (João's taste call): it is small and self-contained, but `bootscene.cpp` changed a lot since (#37, #44, #56), so it needs a rebase and a look on the black deck |
| 4 | **Camera alerts on the bench** | merged (#38); the bench list is BACKLOG "Camera alerts, owed on the bench" | Only compiled and host-tested | **Finish**: run the bench list (NMEA replay, voice pack, `[mem]`) |
| 5 | **Car checks owed** for merged work: PS3 redesign (#57), IR remote (#58), LED welcome (#47), cartoon montage (#56), black karaoke (#55), terminal line (#62) | merged on `main` | Desk-checked only | **Finish** in the car; already listed under BACKLOG "Verification" |
| 6 | **Yellow horn / siren plan** and its wiring diagram | `docs/siren-wiring` (3 commits, **local only, never pushed**) | Plan `docs/superpowers/plans/2026-10-28-yellow-horn.md` and a draw.io diagram; no code. Also adds a `.mcp.json` (Wokwi) | **Merge the docs** (push and PR the plan and diagram, without `.mcp.json`) or drop it if the horn is off the table. The yellow deck's scope (#52) and heap leave little room for it |
| 7 | **IR remote plan**, never committed | main checkout `spotify-pokemon-deck`, untracked `docs/superpowers/plans/2026-11-12-ir-remote.md` | The feature merged in #58 without this plan file | **Done (2026-11-15):** committed on this PR; the untracked copy in the main checkout can go once it merges |
| 8 | **OBD gauge cluster** (companion screen) | `feat/obd` (41 commits, 234 behind, on origin) | Parked 2026-10-23; its OBD core moved to the OBD deck in #30, which merged | **Drop**: BACKLOG's own rule was "retire the branch once car-health merges", and it has. Remove that line from BACKLOG's Code health |
| 9 | **One poll task on the yellow deck** (lyrics, camera band, car health in one task to save heap) | `claude/yellow-heap` (1 `wip` commit, on origin) | The commit itself says it was not enough (~20 KB free, still crashing); the slim (#52) solved the heap another way and took those features off the yellow deck | **Drop** |
| 10 | **ES8311 mic test spike** | `spike/mictest-s3` (1 commit, local only) | Parked; `feat/voice-agent` has the mic diag firmwares on main's mic driver | **Drop** (superseded by #2) |
| 11 | **IR test stash** | stash `On feat/irtest: IR test work in progress` (2026-11-12) | Its `env:irtest` and `src/diag/irtest.cpp` are on `main` since #58 | **Drop** the stash |
| 12 | **Split `main.cpp` into boot, deck view and status view** | `refactor/split-main`, closed PR #11 | Old layout (`src/app/`), before the #26 reorganisation | **Drop**. The live version of the idea is BACKLOG "Split `src/yellow/ledpanel.cpp`" |
| 13 | **Hotspot boot, first try** | `feat/hotspot-boot`, closed PR #12 | Redone and merged as #49 | **Drop** |
| 14 | **Diagnostics not in CI** (`blescan`, `bletest`, `ps3test`, `irtest`) | `.github/workflows`, BACKLOG "Code health" | Open; `irtest` (#58) is a new env CI doesn't build either | **Finish** (small): add them to the matrix or a weekly job |
| 15 | **Yellow deck features removed by the slim** (#52): OTA, lyrics on the yellow deck, car-health screens | `main` (history keeps them; `util/car/healthview` kept) | Deliberate, documented in BACKLOG ("Done", "Ideas") | Nothing to do: kept for the new display deck |

Other switches found on `main`, all deliberate and commented, no action: `MONTAGE_SOUND_AT_START`
(`src/black/ui/bootscene.cpp`), `DUMP_GATT` (`src/diag/blescan.cpp`, "done, off for now"),
`BACK_OFF` per deck (`src/common/net/wifi.cpp`), `BENCH` (`src/obd/carstate.cpp`). The SD card pins
in `include/pins.h` are unused on purpose (the SD mount starves the TLS heap). There are no
`TODO` / `FIXME` / `#if 0` markers in `src/`, `include/` or `tools/`.

## Open PRs

- **#63** `docs/yellow-leds-debug`: debug notes for the yellow lights (one docs file). Open since
  2026-11-14. Merge or close it once the LEDs work settles; the other worker owns that.

## Branches with nothing left to merge

All of these point at the head of a merged PR, or at a commit already on `main`:

- Merged PR heads: `chore/wave1-safety`, `refactor/wave2-folders`, `fix/wave3-reliability`,
  `docs/wave4`, `claude/boot-sound-rosters`, `claude/connect-chime`, `claude/led-welcome`,
  `claude/night-vivid-main`, `claude/no-pad-mark`, `claude/ps3-pad`, `claude/watch-spotify-214e5f`,
  `claude/yellow-menu-balance`, `claude/yellow-slim`, `claude/project-thread-sh4dlk`,
  `chore/pokeface-moved`, `docs/backlog-pending`, `docs/repo-rename`, `feat/black-karaoke`,
  `feat/camera-alerts`, `feat/car-leds`, `feat/hotspot-boot-v2`, `feat/ir-remote`, `feat/jp-lyrics`,
  `feat/music-mode`, `feat/pad-redesign`, `feat/song-colours`, `feat/terminal-line`,
  `feat/yellow-menu`, `feat/yellow-ota`, `fix/bletest`, `fix/cyan-theme`, `fix/deck-address`,
  `fix/lyrics-matching`, `fix/plain-inside-lights`, `fix/yellow-restarts`, `perf/safe-wins`,
  `readme-photos`, `office/q-20261113071813-…`, the `dependabot/…` branches.
- Local-only leftovers at the terminal-line spec/plan commits (`e8c85fd`, `17928ab`), whose files
  are on `main` byte for byte since #62: `claude/keen-jones-97f38e`,
  `claude/nostalgic-davinci-d9f427`, `claude/objective-nobel-82d3ea`,
  `claude/quizzical-rosalind-baeedf`, `claude/strange-satoshi-157050`,
  `claude/vibrant-jemison-7febb3`.
- Local `main` is 2 ahead, 5 behind `origin/main`: the same two docs commits. Resetting it to
  `origin/main` loses nothing.

## Worktrees

None has uncommitted tracked changes or unpushed commits. Ignored files are the generated media in
`data/` (rebuilt by the `tools/gen_*.py` scripts), `.pio/`, caches, `.superpowers/` brainstorm
files and, in `-voice`, `agent/node_modules/`. Each has its own git-ignored `src/config.h`.

**Done (2026-11-15):** the 12 worktrees marked **Yes** were removed with `git worktree remove`
(no `--force`; their branches stay). Their git-ignored `.superpowers/` session notes (brainstorms,
research, task notes, ~6 MB) were copied first to a backup folder outside the repo.

| Worktree | Checked out | State | Safe to clean up? |
|---|---|---|---|
| `spotify-pokemon-deck` (main checkout) | `main` | 2 ahead/5 behind (see above); untracked `.mcp.json` and the IR remote plan (#7) | **Keep.** Save the IR plan first, then fast-forward to `origin/main` |
| `cardeck-pr4` | detached at `d7d4375` (#6's head, 2026-10-21) | stale | **Yes** |
| `cardeck-termline` | `feat/terminal-line` (#62 merged) | clean | **Yes** |
| `spotify-pokemon-deck-camalerts` | `feat/camera-alerts` (#38 merged) | clean | **Yes** (bench check runs from `main`) |
| `spotify-pokemon-deck-cyan` | `fix/cyan-theme` (#61 merged) | clean | **Yes** |
| `spotify-pokemon-deck-deckaddr` | `fix/deck-address` (#60 merged) | clean | **Yes** |
| `spotify-pokemon-deck-health` | `feat/black-karaoke` (#55 merged) | clean | **Yes** |
| `spotify-pokemon-deck-lyrics` | `fix/lyrics-matching` (#36 merged) | clean | **Yes** |
| `spotify-pokemon-deck-menu` | `feat/yellow-menu` (#18 merged) | clean | **Yes** |
| `spotify-pokemon-deck-music` | `docs/backlog-pending` (#32 merged) | clean | **Yes** |
| `spotify-pokemon-deck-ota` | `feat/yellow-ota` (#19 merged) | clean | **Yes** |
| `spotify-pokemon-deck-voice` | `feat/voice-agent` (unmerged, #2) | clean, pushed | **Keep** (or remove and re-create from the branch later; nothing is lost, it is pushed) |
| `.claude/worktrees/ps3-pad-v2` | detached at `4728263` (#53, on `main`) | clean | **Yes** |
| `.claude/worktrees/watch-spotify-214e5f` | `claude/watch-spotify-214e5f` (#48 merged) | clean | **Yes** |
| `.claude/worktrees/night-vivid-d041c8` | `fix/yellow-pad-lights-restart` | another worker's live branch | **No**, in use |
| `.claude/worktrees/led-intro-3f5e0c` | `docs/unfinished-work-audit` (this doc) | this PR | After this PR merges |

## Suggested order

1. ~~Commit the untracked IR remote plan (#7)~~ (done), then fast-forward local `main`.
2. ~~Remove the 12 worktrees marked **Yes**~~ (done), then the merged and leftover branches above.
3. Drop `feat/obd`, `claude/yellow-heap`, `spike/mictest-s3`, `refactor/split-main`,
   `feat/hotspot-boot` and the IR test stash; update BACKLOG's `feat/obd` and "voice agent (local)"
   lines.
4. Decide on the greeting (#3) and the horn docs (#6).
5. After the LEDs work: effects back on (#1), close or merge #63, then the car checks (#4, #5).
