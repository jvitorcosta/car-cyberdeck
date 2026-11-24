# Car Deck review plan (2026-11-24)

**Status:** plan only. Nothing here has been executed: no code changed, nothing flashed, no deck run.
Another session executes it after João's OK. Everything was read from `main` = `3d35e70` (#94).

## Summary

- **What is reviewed:** one repo with three firmwares and a watch app: the black deck (ESP32-S3, env `s3`, `src/black/`),
  the yellow deck (CYD ESP32, env `ledpanel`, `src/yellow/`), the OBD deck (plain ESP32, env `obddeck`, `src/obd/`), shared code
  (`src/common/`, `src/util/`, `lib/irrx`), ~73 files in `tools/` (Python and PowerShell), 140 Unity suites in `test/`, and a
  Monkey C watch app. About 40 K lines of C/C++ in `src/`, 20 K lines of host tests, 7 K lines of Python, 551 commits, 5 tags (last `v2.0.0`).
- **Approach:** 12 review dimensions in 7 phases (0 to 6). Phases 0 to 4 and 6 are read-only on the repo (reading, builds, host
  tests, static analysis on scratch installs). Phase 5 is the only one that touches a deck, passive captures only, and only with
  João's go. The output is one ranked findings file and a list of grouped follow-up PR proposals, not fixes.
- **Memory is the spine.** The yellow deck has ~37 KB free heap and no PSRAM (memory note `yellow-deck-scope`); TLS on the S3 needs
  ~40 KB in one internal block. Every phase asks what a change does to the heap; Phase 2 is entirely about it.
- **First-pass observations (to verify in the review, not conclusions):**
  1. `src/yellow/ledpanel.cpp` is 3,493 lines; `docs/BACKLOG.md` "Code health" still says "~2 800". It is the largest file by 2x.
  2. `CHANGELOG.md` `[Unreleased]` is long and the last tag is `v2.0.0`, although many `feat` commits (gauges, Cockpit, Suki Ride)
     have merged since; the README "Versioning" rule is not followed in practice.
  3. CI builds `s3`, `hwcheck_s3`, `ledpanel`, `hwcheck`, `obddeck`, `obddeck_bench`. It does not build `blescan`, `bletest`,
     `ps3test`, `irtest`, `ledpanel_probe`, `s3_ota`, `obddeck_ota` (BACKLOG flags it; `irtest` is newer than that note).
  4. CI gates flash % (97 %) and the media pack (95 %) but not RAM %, stack high-water marks or heap. No static analysis (no
     cppcheck/clang-tidy), no `.clang-format`, no Python linter, no secret scan. PlatformIO is pinned to 6.2.0, the platform to `espressif32@6.9.0`.
  5. `src/black/fonts/vt323_*.h` and `src/yellow/fonts/vt323_*.h` are near-copies (same size, `diff` reports a difference): drift hazard
     or a candidate for `common/`.
  6. `docs/review/2026-10-08-code-review.md` still refers to `-e esp32dev` and fields from before the reorganisation.
  7. `.gitignore` lists worktree folder names (`night-vivid-d041c8/`, `reviewer-agents-b606f8/`, ...): hygiene smell.

## Table of contents

1. [Scope and ground rules](#1-scope-and-ground-rules)
2. [What exists today](#2-what-exists-today)
3. [Review dimensions: what to check and how](#3-review-dimensions)
4. [Measured on devices vs only read](#4-measured-on-devices-vs-only-read)
5. [Phases: order, Done-when, effort, model](#5-phases)
6. [Parallel work vs shared files](#6-parallel-work-vs-shared-files)
7. [Risks](#7-risks)
8. [Deliverables](#8-deliverables)
9. [Open questions for João](#9-open-questions-for-joão)

---

## 1. Scope and ground rules

**In scope:** whole repo at `main`: `src/` (black, yellow, obd, common, util, diag), `lib/irrx`, `include/`, `platformio.ini`,
`partitions_s3.csv`, `tools/`, `test/`, `.github/`, `.claude/` (skills, reviewer agents), `docs/`, `garmin/carlights`,
`README`/`CONTRIBUTING`/`CHANGELOG`/`SECURITY`/`CLAUDE.md`, `data/media_sizes.txt` (not the git-ignored media).

**Out of scope:** fixing anything; parked branches (`feat/voice-agent`, `claude/intro-greetings-8b8b67`); the private media files;
other repos (`garmin-watchfaces`, `car-can-lab`, `open-radars-manaus`, `haylou-*`).

**Hard rules for the executing session:**
- **Never flash, upload, erase, open a serial port or start a capture** unless João says so in that session for that step. Phase 5 is
  gated by his go, deck by deck, with env and port named (s3 = black deck, `COM12`; ledpanel = yellow, CH340 port from `pio device list`; obddeck = OBD).
- Work in a worktree from `origin/main`. `pio pkg install -l` rewrites `platformio.ini` in a worktree (see
  `.claude/skills/esp32-dev/references/pitfalls.md`): run it on a copy and `git diff` afterwards.
- Never run `uploadmedia` or pack media from a worktree (zero placeholders wipe the real media; memory note `cartoon-montage`).
  CI's `python tools/pack_media.py .pio/media.bin` size check into `.pio/` is fine.
- Never commit `src/config.h`, `data/` media or `include/fw_version.h`. The findings file must not contain secrets, the number plate or personal notes (the repo is public).
- Conventional commits; the review's output is one docs PR.
- Findings are ranked **BLOCKER / MAJOR / MINOR / NIT** (the scale `.claude/agents/esp32-reviewer.md` already uses); each has `file:line`,
  evidence (command output or measured number) and a one-line suggested fix. A finding without evidence is dropped.

## 2. What exists today

| Area | Where | Notes to carry into the review |
|---|---|---|
| Black deck | `src/black/` (`core/ net/ spotify/ lyrics/ images/ pokemon/ audio/ car/ ui/ fonts/ board/s3/`, `main.cpp` 735 lines) | S3, 8 MB PSRAM, 16 MB flash, 2 x 3 MB OTA slots, media in own partition (`partitions_s3.csv`, 0x9E0000) |
| Yellow deck | `src/yellow/` (`ledpanel.cpp` 3,493 lines, `elklink`, `padhid`, `touch`, `wifibufs`, `ui/`) | CYD, no PSRAM, Bluedroid dual mode + WiFi, USB flashing only (`NO_WIFI_UPDATES`), `--wrap` hooks on `ps3_l2cap_send_hid` and `esp_wifi_init` |
| OBD deck | `src/obd/` (`obdlink`, `carstate`, `carfs`, `benchlink`) | NimBLE central, LittleFS trip log, serves `GET /car`, `/fill` |
| Shared Arduino | `src/common/{net,ui,board,core}` | WiFi, OTA, clock, remote settings, watchdog, backlight, crashlog |
| Pure logic | `src/util/{wire,lights,car,common,black,yellow}` | no Arduino; rule: `util/` never includes `ui/` or Arduino; host-tested |
| Diagnostics | `src/diag/*` (6 files, 6 envs) | not all built by CI |
| Tests | `test/` 140 suites via `tools/run_tests.ps1` (Unity + g++, ASan/UBSan, 64- and 32-bit); 14 `tools/test_*.py` | CI job `host-tests` |
| Tools | `tools/`: `gen_*.py` generators, `pack_media.py`, `check_flash.py`, `media_sizes.py`, `version.py`, `capture_serial.py`, `analyze_session.py`, `serial_health.py`, `obd_sim.py`, `obd_probe.py`, `ota_auth.py`, `deck_shot.py`, `tools/obd2/` | stdlib only except pyserial, pillow, bless, bleak (`tools/requirements.txt`, unpinned ranges) |
| CI | `.github/workflows/ci.yml` (jobs `firmware` matrix of 3 and `host-tests`), `dependabot.yml` (actions only), `CODEOWNERS`, templates | actions pinned by SHA, read-only token, concurrency cancel |
| Versioning | `tools/version.py` -> `include/fw_version.h`; tags `v1.0.0`, `v1.1.0`, `v2.0.0`; `CHANGELOG.md` | rule in README "Versioning" |
| Agent kit | `.claude/skills/{esp32-dev,obd2}`, `.claude/agents/{esp32-reviewer,obd2-reviewer}.md`, root `CLAUDE.md` | reuse in the review (§3.12) |
| Docs | `docs/ARCHITECTURE.md` (481 lines), `BACKLOG.md` (409), `HISTORY.md`, `yellow-deck.md` (509), `review/` (2 earlier audits), `debug/`, `hardware/`, `superpowers/{plans,specs}` (57 files) | check against code (drift) |
| Watch | `garmin/carlights/` (Monkey C) | FR255 Connect IQ menu drives the LED panel via `/say` |
| Recent PRs | #90 OBD load every 3 s; #91 Cockpit takes yellow look; #92 PS3 presses wait for the loop; #93/#94 black pink = yellow pink | "host-tested, not run on the car": used as a calibration set (§3.12) |

## 3. Review dimensions

Each dimension: what to check, how, evidence to produce. Commands assume the repo root, PowerShell, `cp include/config.example.h src/config.h`
and `python tools/media_sizes.py placeholders` done (as CI does).

### 3.1 Memory (RAM, heap, stack, PSRAM, flash). Weight: highest

- **Static budget per env.** `pio run -e s3 -e ledpanel -e obddeck`; read the RAM/Flash summary; `pio run -t size -e <env>`;
  `xtensa-esp32-elf-nm --size-sort -S .pio/build/<env>/firmware.elf | tail -60` (toolchain from `~/.platformio/packages`) for the largest symbols (`.bss`, `.data`, fonts, embedded blobs).
  Record DRAM %, IRAM %, app % of slot (S3 0x300000, CYD 0x1E0000), media pack % of 0x9E0000. Compare with CI's 97 % / 95 % gates and the figures in comments (`ledpanel`: "~1.39 MB of 1,966,080").
- **Map file** from a scratch `platformio.ini` copy (`-Wl,-Map,...`): per-object contribution; find duplicated fonts (`vt323_*.h` exist per deck), large non-`const` tables in RAM, `String` use.
- **Allocation hygiene (grep + reading).** `Grep` for `String`, `new`, `malloc`, `std::vector`, `std::string`, `std::function`, `heap_caps`, `mem::bigAlloc` under `src/`.
  First-pass hot spots: `String` in `black/net/remote.cpp` (10 hits), `black/lyrics/lrclib.cpp` (6), `yellow/ledpanel.cpp`, `black/images/walksprite.cpp`, `common/net/wifi.cpp`, `obd/obdlink.cpp`.
  For each: per-request or per-frame (fragmentation) or one-off at boot? freed? bounded? Black deck: CPU-only buffers via `mem::bigAlloc`, no DMA/ISR buffer in PSRAM.
- **Largest contiguous block, not free heap** (`esp32-dev/references/memory-display.md`: `ESP.getFreeHeap()` lies). Check every heap decision uses `mem::byteFree()/byteLargest()` (black) or `heapguard` (yellow).
- **Stacks.** Every `xTaskCreate*`: stack size vs measured high-water mark, core, priority, watchdog registration; big locals (>512 B) on small stacks; recursion.
- **TLS budget.** List every `WiFiClientSecure` user and the gating on the largest block (`black/core/nettask.cpp`, `spotify/client.cpp`, `lyrics/lrclib.cpp`, `genre/apple.cpp`, `images/art.cpp`).
- **Yellow headroom.** Worst case, not idle: PS3 pad + lights + karaoke + music + `/say` together. History to read first: #52 (slim), #66 (`heapguard`), `wifibufs.cpp`.
- **Flash.** Image size trend (BACKLOG/CHANGELOG numbers vs a fresh build), `board_build.embed_files` per env, libraries linked but barely used, unused font flags.
- **Evidence:** per env table (RAM %, flash %, top-20 symbols, task stack table, loop-allocation list).

### 3.2 Architecture and coherence across decks

- **Layering, mechanically.** `Grep` for `#include.*ui/` under `src/util src/black/core src/black/images`; `Arduino.h|WiFi.h|freertos` under `src/util`. Compare `docs/ARCHITECTURE.md` and `CONTRIBUTING.md` "Project layout" claims with the tree.
- **Allow-list `build_src_filter`.** Per env, which folders compile; each `-<file>` exclusion (`http_config.cpp`, `httpget.cpp`, `benchlink.cpp`) justified; files under `src/` compiled by no env at all (`Glob src/**/*.cpp` minus the union of the filters); headers nobody includes.
- **Cross-deck contracts** (`src/util/wire/`: hmac256, lyricstatus, musicwire, sigreq, songpalette, statejson; `util/car/carwire`): one definition shared by both ends, a version or compat rule, tests pinning them. Document the HTTP surface (`/car`, `/fill`, `/obd`, `/say`, `/status`, `/link`, remote settings) and check auth consistency with `common/net/reqauth`.
- **Black vs yellow coherence.** List each concept implemented twice (night/day palette, pink, LED looks, deck links, VT323 fonts, terminal look) and who owns the single source (`util/lights/ledlook`, `util/yellow/termpal`; `black/ui/pink.h` from #94 copies a yellow constant). A duplicate is a finding only if it has drifted or no test ties the copies.
- **Unit size.** Files > 500 lines (`ledpanel.cpp` 3,493, `black/main.cpp` 735, `black/ui/bootscene.cpp` 672, `util/car/sukiride.cpp` 667, `obd/obdlink.cpp` 660) and functions > 100 lines or complexity > 15: `pip install lizard` in scratch, `lizard src -C 15 -L 100`. Re-check the BACKLOG "Split `ledpanel.cpp`" item against the real size.
- **Concurrency model.** Tasks, cores, mutexes, shared state (`include/app_state.h`, `black/core/*`): who writes, who reads, lock scope, locks held across network calls or `delay`, ISR/BLE-callback safety (also the `--wrap` functions).
- **Failure and recovery.** Table "what happens when X fails" (WiFi lost, Spotify 429, adapter lost, NVS full, empty media partition, low heap): 90 s watchdog coverage per task, `bootguard` rollback, `ota::onOnline` confirm, `common/core/crashlog`, yellow `heapguard` restarts.
- **Evidence:** layering-violation list (empty is a valid answer), dead-file list, duplicated-concept table, hotspot table.

### 3.3 Code standards: C++

- **Compiler gate.** `build_src_flags` is already strict (`-Wall -Wextra -Wshadow -Wdouble-promotion -Wlogical-op -Wduplicated-cond -Wnull-dereference -Wformat=2 -Werror`). Check what it does not cover: `irtest` clears it, `lib/irrx` is outside it, diag envs may have rotted. In a scratch build add `-Wconversion -Wsign-conversion -Wold-style-cast -Wcast-qual -Wswitch-enum -Wimplicit-fallthrough=5 -Wcast-align` and count warnings per directory (information, not blockers).
- **Static analysis on scratch installs.** `pio check -e s3 -e ledpanel -e obddeck --severity=medium --skip-packages` (cppcheck); `clang-tidy` with `bugprone-*,performance-*,cert-*` over `pio run -t compiledb` output. Triage real vs noise; keep a suppression list.
- **Idioms by reading, guided by grep:** exceptions/RTTI, `std::function`/`shared_ptr`/iostream (flash and heap), `strcpy/strcat/sprintf` (should be `snprintf`), `atoi` without a range check, `memcpy` with computed sizes, `%s` with possibly-null pointers; `millis()` comparisons without wrap handling (CONTRIBUTING demands a wrap test per timer); `size_t` vs `int` buffer math (CI's 32-bit run catches some); `volatile` vs atomics for cross-task flags; header hygiene; magic numbers (named colours and timings are fine).
- **Style:** CONTRIBUTING says 4-space, camelCase functions, UPPER_CASE constants; `.editorconfig` exists, `.clang-format` does not. Report the inconsistency count; do not reformat.
- **Comments:** the rule is "why, not what"; sample 20 files and flag comments that contradict the code.
- **Evidence:** warning counts per directory per flag, cppcheck/clang-tidy triage table, grep counts.

### 3.4 Code standards: Python and PowerShell

- On a scratch venv: `ruff check tools --select E,F,W,B,UP,S,SIM`, `mypy tools --ignore-missing-imports`, `bandit -r tools -ll`, `vulture tools --min-confidence 80` (dead generators).
- `python -m unittest discover -s tools -p "test_*.py"` plus `coverage run -m unittest ...; coverage report`. List the ~30 untested scripts and rate each: one-shot generator vs recurring tool (`capture_serial.py`, `serial_health.py`, `ota_auth.py`, `check_flash.py`, `version.py` deserve tests).
- Serial/OTA tools: DTR/RTS handling in `capture_serial.py`, `ota_auth.py` never printing `OTA_PASSWORD`, `shell=True`, Windows vs Linux paths and encodings (João runs Windows, CI runs Linux).
- Dependencies: `tools/requirements.txt` is unpinned (`pyserial>=3.5`, `pillow>=12.1`, `bless`, `bleak`); no lock. CONTRIBUTING says generators need only the standard library but `gen_emoji.py` needs pillow: check wording.
- Consistency: top-of-file docstring pattern, `argparse` vs `sys.argv`, exit codes.
- PowerShell: `tools/run_tests.ps1`, `tools/ntest.ps1` with `Invoke-ScriptAnalyzer`; its `#include` scraper is a regex: check it does not miss macro or conditional includes; parity between `pwsh` on Linux (CI) and Windows PowerShell.
- **Evidence:** lint/type/security counts with a triaged top list; tested/untested tool table.

### 3.5 Other languages and config

- **Monkey C (`garmin/carlights`):** read `source/`, `manifest.xml`, `monkey.jungle`: secrets handling (`Secrets.mc` git-ignored), HTTP failure paths, memory on the FR255, manifest permissions. Build only if the Connect IQ SDK exists (Q8).
- **`platformio.ini`** (353 lines): inheritance chain (`common` -> `cyd` -> `ledpanel` -> `ledpanel_probe`; `s3` -> `s3_ota`, `hwcheck_s3`), flags duplicated between envs, comment accuracy, pins (`espressif32@6.9.0`, `TFT_eSPI@2.5.43`, git-pinned `XPT2046_Touchscreen#f956c5d`). `pio project config` per env and diff the resolved flags of `s3` vs `ledpanel`.
- **Partition CSVs:** script an overlap/alignment check of `partitions_s3.csv` (4 KB alignment, ends <= 16 MB). Verify the `ledpanel` comment's claim (same NVS 0x9000/0x5000 and otadata 0xe000 in `min_spiffs.csv` and `huge_app.csv`) against the files in the pinned platform package.
- **Markdown/YAML:** link and file-reference check on docs, `actionlint` on `ci.yml`, `.editorconfig` coverage.
- **Generated data:** `dex_data.inc` (1,048 lines), `car_sprite.inc` (1,520), `status_sprite.inc` (864), `genre_data.inc`, fonts: each has a generator in `tools/`? output reproducible (run twice, same bytes)? checked-in file in sync?

### 3.6 Repo hygiene and cleanup (unused things that degrade the system)

- **Unused code.** (1) a scratch build with `-ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,--print-gc-sections` per env: what the linker drops; (2) `cppcheck --enable=unusedFunction` over the union of each env's filter (cross-deck use matters); (3) files in no env; (4) headers with no includers; (5) `Grep` for `#if 0`, `TODO`, `FIXME`, `XXX`, `HACK` (the 2026-11-15 audit found none: re-run); (6) compile-time switches (`MONTAGE_SOUND_AT_START`, `DUMP_GATT`, `BACK_OFF`, `BENCH`, `LED_EFFECTS 0`, `IR_EFFECT_PROBE`): keep, document or remove.
- **Unused things that cost flash or heap:** blobs per env (`embed_files`), `LOAD_FONT*`/`LOAD_GFXFF` flags vs real draws, libraries in `lib_deps` vs includes (`JPEGDEC`, `PNGdec`, `TJpg_Decoder`, `IRremote`, `XPT2046_Touchscreen`: used by which env?), NimBLE options, `SMOOTH_FONT` in `cyd`.
- **Repo clutter:** worktree names in `.gitignore`; `docs/superpowers/{plans,specs}` (57 files; many for features since reshaped or removed: avatar, companion screen, voice agent): classify live / historical / obsolete; `docs/review/`: close the loop on the 2026-10-08 list and the 2026-11-15 audit (done vs open); remote branches not merged (`git branch -r --no-merged origin/main`) and stashes; `data/` vs `.gitignore` and `data/media_sizes.txt`.
- **Docs vs code drift:** `ARCHITECTURE.md` module lists vs `Glob src/util`; README build commands vs `platformio.ini`; BACKLOG "Done"/"Verification" vs `gh pr list --state merged --limit 100`; CHANGELOG vs commits.
- **Evidence:** table *item, evidence, saves (bytes/lines), risk, suggested PR grouping*.

### 3.7 Performance (CPU, frame time, latency, network)

- **Display.** Black deck: intro at 20 fps (~50 ms/frame per the BACKLOG DMA note), gauge/Suki/Cockpit redraw cost, SPI 40 MHz (80 MHz corrupted, 2026-11-03). Read `ui/` for per-frame allocation, per-frame `String`, `fillScreen`, full-screen redraws vs dirty rectangles. Yellow: touch latency, panel redraw, PS3 press latency (#92 added a log).
- **OBD polling budget.** Re-derive from `util/car/obdplan.cpp`, `test/test_obdplan`, `.claude/skills/obd2/references/polling-budget.md` (6 requests/s; #90 costs RPM/speed 8-12 % of refresh). Replay with `tools/obd_sim.py` on the PC; no deck.
- **Network.** Requests per minute to Spotify/LRCLIB/Apple/art host, back-offs (`netplan::PollBackoff`, `LoginBackoff`), 429 handling (BACKLOG "Spotify rate limit"), TLS handshakes, HTTP/1.0 + filtered-stream parsing wherever large JSON arrives.
- **Boot time:** intro/montage/opener on the S3, time to first Spotify poll, yellow time to a usable panel.
- **Evidence:** per-frame/per-request cost table from captures if Phase 5 runs, otherwise code-reading estimates marked "estimate".

### 3.8 Versioning and releases

- README rule: major = needs action, minor = `feat`, patch = `fix`/`perf`. Compute the bump the rule would have produced: `git log v2.0.0..main --format=%s`, count `feat` vs `fix`; size of `[Unreleased]`; `python tools/version.py` output (builds show `2.0.0+N.g...`); behaviour with CI's `fetch-depth: 0` vs a shallow clone (`0.0.0-dev`).
- **Cross-device compatibility.** Is there a version or handshake between black <-> yellow <-> OBD (`/car`, `/status`)? What happens when decks run different versions? Skew has already bitten once (memory `boot-sound-rosters`: an old branch was flashed).
- **Release process.** CI runs on `v*` tags but only builds. Are binaries/checksums published? Is the changelog section written by hand? Evaluate a minimal `release.yml` (build the three firmwares, attach `.bin` + SHA-256, no media) as a suggestion only.
- **Dependency pins.** Table of every pin (`platform`, libs, `platformio==6.2.0`, Python 3.12, action SHAs): what is outdated (`pio pkg outdated`), what is held on purpose (the reviewers' rule: no `espressif32@6.9.0` or lib bump without a heap measurement).
- **Evidence:** commits-since-tag by type, skew-detection findings, pin table.

### 3.9 CI/CD and test strategy

- **Env coverage:** `pio project config | grep '^\[env'` vs the CI matrix. Evaluate adding `blescan`, `bletest`, `ps3test`, `irtest`, `ledpanel_probe`, `s3_ota`, `obddeck_ota` to the matrix or a weekly job; cost from `gh run list --workflow ci.yml -L 20 --json databaseId,conclusion,createdAt,updatedAt`.
- **Missing gates:** RAM % (`size` on the `.elf`), binary-size delta vs main as a PR comment, warning budget, static analysis, Python lint, `actionlint`, markdown link check, secret scan (public repo, "never commit config/media" rule), dependency updates beyond actions (Dependabot covers only `github-actions`), firmware `.elf`/`.map` kept as CI artefacts (today nothing is uploaded, so a heap regression cannot be bisected from CI).
- **Test suite quality.** Run `pwsh tools/run_tests.ps1` (pass/total, duration), then host coverage (`NTEST_FLAGS="-m64 --coverage"`-style, `gcovr`) for `src/util/**` and the pure UI modules. Check the docs' claim "every module in `src/util/` has a suite" by script; list logic outside `util/` with no test (`obd/carstate.cpp`, `common/net/*`, `black/core/nettask.cpp`) and rate the cost of making it testable. Check a sample of >= 15 suites against CONTRIBUTING's rules (millis wrap test, buffer-limit test, parser cut-off/absurd/32-bit-offset inputs).
- **Reliability:** run the suite 3x and under `-m32` and sanitizers as CI does; list flaky or time/order dependent suites.
- **Hardware-in-the-loop gaps:** what only a deck can test (§4) vs what could be simulated (Wokwi MCP noted in memory `design-tools-mcp`, `obd_sim.py`, `nmea_replay.py`, `util/car/benchdrive`).
- **Workflow security:** `permissions: contents: read`, SHA-pinned actions, `persist-credentials: false`, Dependabot `cooldown` are good; check fork PR behaviour (no secrets), cache keys, unpinned `pip` transitive deps.
- **Evidence:** CI gap table (env/gate, present?, cost to add), coverage numbers, flaky list.

### 3.10 UX, UI and user-focused design

The users are one driver and passengers, day and night, with glances of 1 to 2 s. Review as a legibility and driver-distraction problem.

- **Screen and interaction inventory** per deck: black (Now playing, lyrics/karaoke, status, OTA screen, boot intro/montage, GAUGES: Suki, Ride, Drive, Trip, Cockpit; phone page switches; IR remote; camera alert band), yellow (LED panel: colours, intensity, presets, karaoke/music modes, PS3 pad, terminal line, link tags, menu), watch app. Sources: `docs/ARCHITECTURE.md`, `docs/yellow-deck.md`, README, `ui/` folders. `tools/deck_shot.py` (`SHOT`) grabs screenshots from a running deck (Phase 5 only); `tools/suki_ride_preview.py` and `tools/intro_check.py` render without a deck.
- **Glanceability.** Text size and contrast ratio on the real RGB565 colours (script over `util/yellow/termpal`, `black/ui/theme.h`, `black/ui/pink.h`; some suites already test contrast), items per driving screen, colour-only encoding (check the Cockpit alert pairs colour with an outline/word), flicker and redraw artefacts, night brightness (full backlight is a decision: memory `backlight-full`), what animates while the car moves.
- **Safety while driving.** Anything needing touch while moving (yellow touch panel, remote, PS3 pad guards `padguard`/`tapguard`/`touchguard`); "pick COCKPIT while parked" (#87) enforced in code, not only in copy.
- **Feedback and errors.** NO LINK / KEY ON / CHK pill; what the user sees on WiFi loss, Spotify failure, adapter lost, a low-heap restart in a moving car, boot loops, OTA; wording consistency and names across README, CHANGELOG and UI (CITY DATALINK, Cockpit, Suki, Ride); UI language (English copy, Japanese lyrics: CJK font coverage tests exist).
- **Black vs yellow consistency** (#89, #91, #93, #94 are converging them): table of remaining divergences (palette, type, rule thickness, alert style, night theme), intentional vs drift.
- **Phone page and watch.** HTML served by `black/net/remote.cpp`: touch targets (>= 44 px), viewport, contrast, escaping of echoed text, state-changing GETs (`/say`, `/link`); watch menu depth and font sizes.
- **Accessibility.** Colour-blind safety (protan/deutan simulation on the theme colours with a script), minimum font px on the 2.8" panels.
- **Evidence:** screen inventory, contrast table, divergence table, "interaction while moving" list.

### 3.11 Security and privacy (the repo is public)

- **Secrets.** `gitleaks detect` over full history; `git log -p -S` for `OTA_PASSWORD`, `Bearer`, `client_secret`; confirm `.gitignore` covers `src/config.h`, `Secrets.mc`, personal media, plates; `CREDITS.md` vs `data/`.
- **Network surface.** Table of every listener (`/say`, `/link`, `/status`, `/car`, `/fill` signed, `/obd`, OTA port 3232): auth (`common/net/reqauth`, `util/wire/hmac256`, `sigreq`), replay window, state-changing open endpoints, plain http on the hotspot (documented), which TLS clients still `setInsecure()` and why (a CA bundle exists: `common/net/ca_bundle.inc`).
- **OTA.** `ota_auth.py`, password handling, SHA-256 check, rollback; what someone on the hotspot can do.
- **Input handling.** Parsers of outside data (Spotify JSON, LRC, Apple responses, NMEA, ELM327, HTTP heads): length checks, bounded buffers, no `sscanf("%s")`; verify the repo's own rule (cut-off, absurd number, 32-bit wrap test) per parser.
- **Repo settings.** `SECURITY.md`, `CODEOWNERS`; branch protection and secret scanning are not visible from the repo (Q7).
- **Evidence:** gitleaks result, listener table, parser table with test status.

### 3.12 Process, docs and the agent kit

- **Calibration set #90 to #94.** Per PR: tests added? docs/CHANGELOG/ARCHITECTURE updated in the same PR? "not run on the car" tracked in BACKLOG "Verification"? Flash-hazard notes (memory `s3-flash-drops`, `s3-usb-serial-stall`, `ride-art-standin`; is #86's guard tested in `test_mediapack`)? Use it to ask whether these dimensions would have caught anything.
- **Owed car checks.** BACKLOG "Verification (on device)" and "Car test" lists: count, age, which merged PRs depend on them (#47, #55, #57, #58, #62, #69/#70, #71 to #75, #81/#82, #83): a verified-vs-owed matrix.
- **Docs health.** Link rot; duplicate truth (README vs ARCHITECTURE vs yellow-deck vs CLAUDE.md); are `CLAUDE.md`, the two skills and two agents accurate with today's envs and ports; do CONTRIBUTING's commands work on a fresh clone (follow them literally in a scratch clone).
- **Agent kit self-test.** Run `esp32-reviewer` on #91 and #92 and `obd2-reviewer` on #90 (both read-only), compare with this plan's dimensions; update their checklists only as a later PR.
- **Evidence:** PR hygiene table (#90 to #94), verification matrix, docs-drift list.

## 4. Measured on devices vs only read

| Needs a deck (Phase 5, João's go per deck) | No deck needed (Phases 0 to 4) |
|---|---|
| Runtime heap: free, **largest block**, minimum over 10 to 30 min per deck in realistic modes (`tools/capture_serial.py <port> 600 session.log`, then `analyze_session.py`, `serial_health.py`): resets, crashes, watchdog, lowest heap, stack HWM | Static: map/size/symbols per env, `.bss`, flash %, partitions |
| Black: PSRAM free, GAUGES frame times, Spotify poll cadence, TLS failures (`-32512`), `[mem]` lines | Host suites (64-bit ASan/UBSan, 32-bit), coverage, static analysis, lint |
| Yellow: heap under PS3 pad + lights + karaoke + music + `/say` (the ~37 KB budget), `heapguard` restarts, PS3 press latency | Layering, dead code, duplication, docs/versioning drift, CI gaps |
| OBD: LittleFS trip log size, `/car` latency on the hotspot, BLE reconnect (bench env `obddeck_bench`; the real adapter only in the car) | Simulated drives (`util/car/benchdrive`, `tools/obd_sim.py`, `tools/nmea_replay.py`) |
| Screenshots (`tools/deck_shot.py`), legibility judged by eye at night | Contrast and colour-blind scripts on palette constants |
| Boot timing, OTA path (never flash during the review) | Parser and wire-format reasoning, security surface |

Metrics recorded per env, kept in the findings file so a later run can diff: app flash bytes and %, DRAM %, largest free block (min), free internal heap (min), free PSRAM (S3), task stack HWM, allocation-failure count, resets/crashes/watchdogs per capture, TLS failures, warnings under the extra flags, suites (count/pass/duration), line coverage of `util/`, CI minutes per job.

## 5. Phases

Model tiers: **opus** = judgment and design, **sonnet** = structured analysis and scripted work. Effort is the executing session's wall-clock.

### Phase 0: Baseline and setup (read-only, ~1 h, sonnet, low effort)
- Fresh worktree from `origin/main`; `cp include/config.example.h src/config.h`; `python tools/media_sizes.py placeholders`.
- `pio run` for `s3`, `ledpanel`, `obddeck`, `hwcheck`, `hwcheck_s3`, `obddeck_bench`, then **build only** the envs CI skips (`blescan`, `bletest`, `ps3test`, `irtest`, `ledpanel_probe`, `s3_ota`, `obddeck_ota`). No `-t upload` anywhere.
- `pwsh tools/run_tests.ps1`; `NTEST_FLAGS=-m32 NTEST_SAN=0 pwsh tools/run_tests.ps1`; `python -m unittest discover -s tools -p "test_*.py"`. Save raw output in the scratchpad.
- **Done when:** a baseline table exists (build pass/fail per env, RAM/flash per env, suites pass/total and duration, tool-test count); every env that fails to build is already a finding.

### Phase 1: Static mechanical sweeps (read-only, ~3 h, sonnet, medium)
Dimensions 3.3, 3.4, 3.5, 3.6 (dead code), 3.9 (CI gaps, coverage), 3.11 (gitleaks, listener table), mechanical part of 3.2. Run the §3 tools on scratch installs (nothing added to `requirements.txt`, nothing committed).
- **Done when:** each tool's raw output is saved and a triaged list (true positive / noise / needs a human) with counts is ready; no tool config or code committed.

### Phase 2: Memory deep dive, static (read-only, ~4 h, opus, medium-high)
Dimension 3.1 without a deck.
- **Done when:** per-env memory table complete; every allocation site in a loop or per request has a bounded/unbounded verdict; every task has a stack/core/watchdog row; at least 5 concrete reclaim opportunities each with bytes and risk, or an explicit "none found".

### Phase 3: Architecture, performance, UX, versioning, process by reading (read-only, ~5 h, opus, high)
Dimensions 3.2 (judgment), 3.7 (estimates), 3.8, 3.10, 3.12, including the reviewer-kit self-test.
- **Done when:** duplicated-concept table, failure/recovery table, screen inventory with contrast and colour-blind numbers, black-vs-yellow divergence table, version-drift numbers, verified-vs-owed matrix, docs-drift list.

### Phase 4: Cleanup, CI and release proposals (read-only, ~2 h, sonnet, medium)
Dimensions 3.6 (docs/branches/specs), 3.8 (release workflow), 3.9 (CI jobs and cost). Turn Phases 1 to 3 into grouped, independently shippable PR proposals (scope, files, savings, verification, risk, needs-a-flash yes/no).
- **Done when:** a ranked list of <= 15 proposals, each citing evidence from an earlier phase.

### Phase 5: On-device measurement (only with João's go, ~2 to 3 h of his time, sonnet drives, João plugs in)
1. Ask what each deck runs (`[boot] cardeck v... (env)`, `/status`). If a deck runs an old branch, stop: the numbers would describe the wrong code.
2. Passive capture, **one deck at a time**, port named by João (`pio device list`). S3 needs DTR/RTS off (the tool does it); never `setTxTimeoutMs(0)`; check no other session holds the port. 10 min minimum per deck in the §4 modes.
3. `analyze_session.py` and `serial_health.py` on each capture; fill the §4 table.
4. Black-deck `SHOT` screenshots only if João wants the UX evidence (retry on failure, per pitfalls).
- Never in Phase 5: flashing, `uploadmedia`, OTA, car tests, real-adapter OBD tests, NVS changes.
- **Done when:** one capture per deck with the §4 table filled, or "skipped by João" for that deck; no deck reflashed; the numbers saved as a baseline for future regressions.

### Phase 6: Synthesis (~2 h, opus, medium)
Write `docs/review/2026-11-<date>-code-review.md` (beside the two earlier audits): top-10 summary, ranked findings, metrics baseline, PR proposals with grouping, the verified-vs-owed matrix, and what was not reviewed and why. One docs-only PR.
- **Done when:** the PR is open and green; every finding has `file:line` + evidence; João has a one-screen summary and picks which proposals become PRs. Executing proposals is a separate later step.

Total: about 2 working days of the executing session for Phases 0 to 4 and 6; Phase 5 depends on João's availability.

## 6. Parallel work vs shared files

Phases 0 to 4 are read-only on the repo (output goes to the scratchpad), so they can run as parallel subagents in one session (max 3 at a time, office rule). Suggested lanes after Phase 0:

| Lane | Dimensions | Reads | Writes |
|---|---|---|---|
| A. Firmware static | 3.1, 3.2, 3.3, 3.6 (src dead code) | `src/`, `include/`, `lib/`, `platformio.ini` | scratchpad |
| B. Tools, CI, release | 3.4, 3.5 (ini, partitions), 3.8, 3.9, 3.11 | `tools/`, `test/`, `.github/`, `partitions_s3.csv` | scratchpad |
| C. Experience and docs | 3.10, 3.12, 3.6 (docs, branches) | `docs/`, README, `src/*/ui/`, `garmin/` | scratchpad |

Shared points: (1) `.pio/` (two lanes running `pio run` at once clash: serialise builds, or use separate worktrees), (2) `platformio.ini` and `src/config.h` (scratch copies only), (3) the findings file (Phase 6, one writer). Phase 5 uses serial ports: strictly sequential and never alongside another flashing session. A `decks-flash` session exists in this workspace today: check `list_sessions` before starting.

Later fix PRs split by file ownership: `ledpanel.cpp`, `src/black/ui`, `tools/`, `ci.yml`, docs are independent; anything touching `platformio.ini` or `src/util/wire/` affects both decks and goes one at a time with both decks rebuilt.

## 7. Risks

| Risk | Likelihood / impact | Mitigation |
|---|---|---|
| Executing session flashes, uploads or opens a serial port by reflex | Medium / **high** (a working deck lost; S3 USB drops need a reseat; yellow is USB-only) | Forbidden outside Phase 5; Phases 0 to 4 use build/test/grep only; no `-t upload`, `uploadmedia`, `esptool` |
| Phase 5 capture disturbs a deck (DTR/RTS reboot on S3, held COM12, tx-timeout stall) | Medium / medium | Tool flags, one deck at a time, João present, skip a deck whose firmware is unknown |
| Measured heap comes from an old branch | Medium / medium | Phase 5 step 1 |
| Media packed from a worktree wipes the boot rosters | Low / high | Only the `.pio/media.bin` size check; never `uploadmedia` |
| `pio pkg install` rewrites `platformio.ini` in the worktree | Medium / low | Scratch copy; `git diff` before finishing |
| Scratch-tool noise becomes "findings" | High / low | Evidence rule; tools never added to the repo by the review PR |
| Later fixes regress heap, timing or the shared wire formats | Medium / **high** | Review outputs proposals only; each later PR carries before/after §4 metrics and a device capture for allocation changes (CONTRIBUTING "On the device") |
| Review drifts into fixing | High / medium | Out-of-scope rule; the findings file is the only artefact |
| Leakage in the public findings file (plate, secrets, personal paths) | Low / high | Quote `file:line`, not values; grep the file for secrets before the PR |
| Time sink on media/generator tools that matter little | Medium / low | Weight effort by dimension: memory, CI/tests and UX first |

## 8. Deliverables

1. `docs/review/2026-11-<date>-code-review.md` (Phase 6): findings, baseline tables, proposals. One docs-only PR.
2. Scratchpad bundle (not committed): raw tool outputs, map files, captures, referenced by name with the commands that produced them so any number can be reproduced.
3. A metrics baseline table (§4) inside the findings file for the next review to diff against.
4. A ranked, grouped list of follow-up PRs (not opened).

## 9. Open questions for João

1. **Phase 5 (on-device):** do you want it, and which decks may be captured passively (S3 on `COM12`, yellow CH340, OBD)? Which firmware runs on each now?
2. **Depth:** is "read-only plus scratch tools" right, or should the review also propose permanent additions (`.clang-format`, cppcheck/ruff/gitleaks in CI, a RAM-% gate, `.elf`/`.map` CI artefacts)? The plan only proposes them.
3. **Versioning:** ship `v2.1.0` or later (`[Unreleased]` is large), or is "one release per season" intentional? Should the decks detect version skew with each other?
4. **Parked work:** skip `feat/voice-agent`, `claude/intro-greetings-8b8b67` and the yellow horn plan (default), or include them in the cleanup classification?
5. **Docs cleanup:** may the review recommend archiving or deleting obsolete `docs/superpowers/{plans,specs}` (avatar, companion, voice), or are they history to keep?
6. **Yellow budget:** treat the ~37 KB heap as a hard constraint on every recommendation, including the `ledpanel.cpp` split (default yes)?
7. **GitHub settings:** can you confirm branch protection, required checks and secret scanning (not visible from the repo)?
8. **Tools on your PC:** Connect IQ SDK, `gitleaks`, `cppcheck`, `clang-tidy`, the PlatformIO `xtensa-esp32-elf` binutils: install in scratch on demand, or skip the checks that need them?
9. **UX focus:** driver-safety (what is on screen while moving), visual consistency (black vs yellow convergence), or both equally?
10. **Cost ceiling:** Phases 2 and 3 are the expensive ones (opus, high effort). Fine, or cap at medium and accept less depth on architecture and UX?
