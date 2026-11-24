# Car Deck code review (2026-11-24)

Whole-repo review of `main` at `2a77658` (after #95), run from `docs/plans/2026-11-24-car-deck-review-plan.md`,
Phases 0 to 4 and 6. **Nothing was flashed, uploaded, erased or captured, no serial port was opened, and Phase 5
(device measurements) was not run.** Every runtime number (heap, stack high-water, frame time, latency) is therefore
"not measured"; where a finding depends on one, it says so. Nothing in the repo was changed except this file.

How to read it: findings are ranked BLOCKER / MAJOR / MINOR / NIT (the scale in `.claude/agents/esp32-reviewer.md`).
Each has `file:line`, evidence (a command and its output, or the line quoted) and a one-line fix. "Needs a flash" says
whether the fix can only be verified on a deck. The yellow deck's ~37 KB free heap is treated as a hard constraint on
every fix. Raw tool output was kept outside the repo; each number can be reproduced with the command beside it.

## 1. Summary: top 10

**No BLOCKER. 4 MAJOR, 32 MINOR, 14 NIT** (section 2). The firmware is mechanically clean: all 13 envs that
were built compile with `-Werror`, 140/140 host suites pass (64-bit, ASan/UBSan), no `sprintf`/`strcat`/exceptions/`std::function`
in `src/`, every `millis()` deadline is wrap-safe, cppcheck found no real defect, no secret in history, every one of
the 180 `.cpp` files is compiled by some env. What is weak is the **margin on the yellow deck**, **process around the
car checks**, **docs drift** and **guard rails around destructive tools**.

| # | Sev | Finding | Why it ranks here |
|---|---|---|---|
| 1 | MAJOR | `uploadmedia` has no guard against zero-filled stand-in media (F-01) | One command from wiping the real media partition; it has happened once already. Fix is host-only |
| 2 | MAJOR | A quiet self-heal restart after 15 min without WiFi ignores GAUGES and the live OBD link (F-02) | In the car with the hotspot off, the cluster would blank and reboot every 15 min. Read from code, not observed |
| 3 | MAJOR | BACKLOG tracks 5 of 13 owed car checks and still says the OBD link never met an adapter (F-03) | The gauge work (#69 to #94) is merged and unverified on the road, with no list of what to look at |
| 4 | MAJOR | README never mentions the gauges; ARCHITECTURE lags by 35 util modules and one screen (F-04) | First-read docs describe a deck that no longer exists |
| 5 | MINOR | Yellow: the 4 KB music task is created on the first MUSIC tap with no heap check, and heapguard's 18 KB start line sits on the 15.3 to 16.4 KB lows of the #66 panics (F-05, F-06) | The two places where the 37 KB constraint is thinnest. Both fixes cost 0 B |
| 6 | MINOR | Black: the hourly token refresh opens a second TLS session while the API session is still up, about 20 KB internal twice for a moment (F-07) | One-line fix (`s_api.stop()` first) that returns ~20 KB at the riskiest instant |
| 7 | MINOR | CI is not a required check, 7 of 14 envs are never built, the Actions cache is at the 10 GiB cap (F-10, F-11, F-12) | A red or stale build can merge; fixes are a few workflow lines |
| 8 | MINOR | No release since `v2.0.0`: 168 commits, two `feat!`, README rule gives v3.0.0; GitHub releases carry 0 binaries (F-13) | Versioning rule not followed; no way to roll back to a named build |
| 9 | MINOR | Cockpit alert vs text contrast is 3.0:1 on the default green theme (deutan dE 27); the 17 and 19 px faces are below an in-vehicle minimum and carry red link messages (F-18, F-19) | Driver-safety legibility, judged by script, to be confirmed on a panel |
| 10 | MINOR | Yellow `/say` accepts POST (`HTTP_ANY`) and WebServer buffers POST bodies twice before the key check (F-08); `/fill` parses `6,49` differently on the two decks (F-09) | Heap-DoS from the hotspot on the weakest deck, and a silently dropped fuel price |

## 2. Findings, ranked

IDs are stable so section 5 can cite them (MAJOR F-01 to F-04, MINOR F-05 to F-36, NIT N-01 to N-14). "Needs a flash" is
"no" unless stated.

### MAJOR

#### F-01 `uploadmedia` accepts zero-filled stand-in media
- **Where:** `tools/media_upload.py:31-45` (the only pre-action is on `upload`), `tools/pack_media.py:26` and `:60` (`STAND_IN = 8`), `tools/media_sizes.py:62-77` (`placeholders()`).
- **Evidence:** `media_sizes.py placeholders` writes full-size zero files (`head + bytes(size - len(head))`). `pack_media` skips only files of 8 bytes or less (`os.path.getsize(path) <= STAND_IN`, line 60), so a worktree or CI stand-in of real size is packed, and `uploadmedia` flashes it at the media partition. I read `media_upload.py` in full: `env.AddPreAction("upload", _check_ride_art)` is attached to `upload` only, and checks `sukiride.bin` only. The review worktree itself holds all 25 media files at full size after the CI-style setup.
- **Fix:** refuse in `pack_media.py` (or a pre-action on `uploadmedia`) when a packed file is all zero in its first 4 KB or lacks its magic, reusing `media_sizes.ride_art_problem`'s approach; one test in `test_pack_media.py`.
- **Heap/flash risk of fix:** none (host tool). Needs a flash: no.

#### F-02 Quiet self-heal restart ignores GAUGES and the live OBD link
- **Where:** `src/black/core/nettask.cpp:281-286` (restart), `src/util/black/netplan.h:136` (`WIFI_DOWN_RESTART_MS = 900000`), `nettask.cpp:64-68` (`selfHealRestart` is quiet).
- **Evidence:** `s_health.onPoll(ok, wifiUp, starved, now) == Restart` -> `selfHealRestart()`; the log line reads "WiFi down for 15 min". `grep gauges nettask.cpp` has no hit, and nothing in `Health::onPoll` reads the OBD state, although the OBD link is BLE and needs no WiFi. After the restart the screen shows only the boot; no reason is drawn.
- **Fix:** while GAUGES is on and the OBD link is Live, skip the 15-minute WiFi-down backstop (keep the heap-starved one), or defer it until speed is 0.
- **Heap/flash risk of fix:** none (a bool in the net task; black deck only). Needs a flash: yes (time to the first gauge, and whether it really restarts after 15 min with the hotspot off). Source: lane read of code, not observed.

#### F-03 BACKLOG tracks 5 of 13 owed car checks; OBD sections are stale
- **Where:** `docs/BACKLOG.md:33-126` (Verification), `:161-205` (OBD deck first flash), `:228`, `:259`.
- **Evidence:** `grep -o '#[0-9]\+' docs/BACKLOG.md | sort -u` finds no PR after #56 while main is at #95; "Suki", "Cockpit", "welcome", "drive log" have 0 hits; last commit of the file is #66. PR #90 says "not run on the car", #94 says "not flashed yet". `BACKLOG.md:161` says the OBD deck "has never run on a board". The matrix is in section 6.
- **Fix:** one "Car checks owed" table (PR, what to look at, merged date, checked yes/no), updated by each PR; mark the Vgate and first-flash sections historical.
- **Heap/flash risk of fix:** none. Needs a flash: no.

#### F-04 README and ARCHITECTURE lag the gauge work
- **Where:** `README.md` (whole), `docs/ARCHITECTURE.md:4-5`, `:305-343`, `:435-465`.
- **Evidence:** `grep -c` README: "Cockpit" 0, "GAUGES" 0, "OBD" 0, "Vgate" 0 (CHANGELOG has them). ARCHITECTURE:4-5 says the OBD deck is "being built on `feat/car-health`" (merged as #30). The Ride screen (#81) appears nowhere. A header-basename script over `src/util` (121 modules) finds 35 not listed (21 of 32 in `car/`), and `termpal` listed under yellow although it lives in `util/common/`.
- **Fix:** add a README "Live gauges" bullet, drop the `feat/car-health` sentence, add Ride, regenerate the module lists from `ls` (a 10-line script, or a CI check that every `util/**/*.h` appears in ARCHITECTURE).
- **Heap/flash risk of fix:** none. Needs a flash: no.

### MINOR: memory and firmware

#### F-05 Yellow: the music task is created lazily with no heap gate
- **Where:** `src/yellow/ledpanel.cpp:2655` (`xTaskCreatePinnedToCore(musicTask, "music", 4096, ...)` in `setMusic`); gate only in `deckGet` `:2268`.
- **Evidence:** `grep -n xTaskCreate src/yellow/ledpanel.cpp`: deck 5120 (`:2497`), music 4096 (`:2655`, lazy, never deleted), voice 4096 (`:3049`), leds-link 4096 (`:3185`). The ~37 KB figure (CHANGELOG, `BACKLOG.md:14`) is an idle number before the first MUSIC tap; after it, the worst case is ~4.4 KB lower (stack plus TCB), and heapguard never sees the allocation.
- **Fix:** create the task at boot (so every measurement includes it) or check `heapGate.room()` first and show `DECK LOW ON MEMORY`.
- **Heap/flash risk of fix:** gate variant 0 B; boot variant makes idle headroom read 4.4 KB lower but the worst case is unchanged. Needs a flash: yes (`[mem] min` after MUSIC ON with pad and both lights linked).

#### F-06 Yellow: heapguard's 18 KB start line is on the heap level of the crashes it was built for
- **Where:** `src/util/yellow/heapguard.h:17-19`, `docs/yellow-deck.md:466-473`, `src/yellow/ledpanel.cpp:2268-2276`, `:2290` (`deckSearch`).
- **Evidence:** the three 2026-11-15 panics logged `heap 15660 (min 15300, block 124)`, `16292 (min 15396, block 236)`, `19768 (min 16384, block 4084)`. An ask may start at free >= 18000 and block >= 6000 and takes "a few KB"; one that starts at 18 KB can bottom out at 14 to 15 KB. `deckSearch` gates on the stale `heapGate.low` flag, not a fresh `room()`, and starts an mDNS query. A reading of the margin, not a proven failure (the crashlog sample is 1 Hz).
- **Fix:** read `ESP.getMinFreeHeap()` after an ask over a drive with pad, lights, music and karaoke, then raise `MIN_FREE` (~22 KB) or measure one ask's dip; make `deckSearch` call `room()`.
- **Heap/flash risk of fix:** none (constants). Needs a flash: yes. Not measured.

#### F-07 Black: hourly token refresh opens a second TLS session while `s_api` is still connected
- **Where:** `src/black/spotify/client.cpp:159-161` (`!tokenFresh()` -> `login()`), `:87-88`, `:31`, `:176-180`.
- **Evidence:** `apiCall` calls `login()` with `s_api` kept open (`setReuse(true)`, `:197`); the 401 path does `s_api.stop()` first (`:176-180`), the expiry path does not. `netplan.h:188-191`: one session costs a 16.7 KB input buffer plus ~3 KB per handshake, gate `TLS_NEED = 20000`; no gate runs before `login()`.
- **Fix:** `s_api.stop()` before `login()` in the `!tokenFresh()` branch; one extra handshake per hour.
- **Heap/flash risk of fix:** frees ~20 KB internal at the refresh instant. Needs a flash: yes (`[mem] largest` around "access token OK").

#### F-08 Unauthenticated POST bodies are buffered twice before the key check; `/say` is `HTTP_ANY`
- **Where:** `src/yellow/ledpanel.cpp:3044`, `src/black/net/remote.cpp:425-440`, framework `WebServer/src/Parsing.cpp:201,217`.
- **Evidence:** the library mallocs whatever arrives (up to a 5 s wait), then copies it into a `String`, before the handler runs `reqauth::pass`; peak = 2 x body in internal heap. The watch uses GET only (`garmin/carlights/source/Deck.mc:137`, `Tunes.mc:74`). A 20 KB POST on a yellow deck with ~37 KB free dips below the 15.3 KB crash level. Requires being on the hotspot.
- **Fix:** `HTTP_GET` for `/say`.
- **Heap/flash risk of fix:** none. Needs a flash: yes to measure the dip.

#### F-09 `/fill` is parsed two ways on the two decks
- **Where:** `src/obd/main.cpp:48-59` (`strtof`) vs `src/black/net/remote.cpp:283-290` + `src/util/car/tripbook.cpp:38-60`.
- **Evidence:** black accepts `6,49`; the OBD deck stops at `,`, drops the price and answers `ok none`; `strtof` also accepts `inf`, `nan`, `6.5e0`. The OBD deck's `/car` has no reader in the repo (`src/obd/main.cpp:5` and `carwire.h:7` still say the yellow deck polls it; that consumer was removed in #52).
- **Fix:** call `tripbook::parsePrice` in `obd/main.cpp`; host test with `6,49`; fix the two stale comments.
- **Heap/flash risk of fix:** none. Needs a flash: no.

#### F-14 Black: I2S RX is installed on every clip but nothing reads the mic (~4.1 KB internal DMA)
- **Where:** `src/black/board/s3/audioport.cpp:30-39`, `src/black/board/s3/mic_es8311.cpp`, `sink_es8311.cpp:35`.
- **Evidence:** `grep -rnE "\bmic::" src test` has no hit outside `AUDIO_PROBE`, which no env sets; `nm` of the s3 elf has no `mic::` symbol. The RX DMA (4 x 256 frames x 2 ch x 2 B) is allocated at `i2s_driver_install` during each clip and the 20 s linger, from the internal heap that TLS needs contiguous.
- **Fix:** TX-only driver; keep RX behind `AUDIO_PROBE`.
- **Heap/flash risk of fix:** frees ~4.1 KB internal while sound is up; flash 0. Needs a flash: yes (a clip at 16 and 48 kHz; the codec may need the RX clock).

#### F-15 Black: refresh body is an Arduino `String` held across the TLS handshake
- **Where:** `src/black/spotify/client.cpp:94-96`.
- **Evidence:** a 280 B body built from 4 to 5 chained temporaries; the file's own comment (`:33-35`) records that a similar `String` capped the largest block at ~17 to 22 KB, below `TLS_NEED`.
- **Fix:** `snprintf` into `char body[320]`.
- **Heap/flash risk of fix:** -~300 B transient heap, +320 B net-task stack (~3.7 KB spare per `nettask.cpp:318-320`). Needs a flash: yes.

#### F-16 Four functions over 100 lines; black `loop()` has CCN 100
- **Where:** `src/black/main.cpp:453-735`, `src/yellow/ledpanel.cpp:1970-2131` (`handlePad`), `src/black/ui/bootscene.cpp:489-638`, `ledpanel.cpp:3361-3493`.
- **Evidence:** `lizard src -C 15 -L 100`: black `loop` NLOC 252 CCN 100; `handlePad` CCN 64; `bootscene::task` CCN 54; yellow `loop` CCN 46; 97 functions have CCN > 15 of 1,896. `ledpanel.cpp` is 3,493 lines (`BACKLOG.md:130` says ~2,800), with 13 `// ----` section seams that map to translation units.
- **Fix:** extract per-screen branches of `loop()` and the pad state machine into `util/` with host tests; split `ledpanel.cpp` along its seams.
- **Heap/flash risk of fix:** neutral if file statics stay statics and no `std::function` or `String` is added. Needs a flash: yes (pad feel, `[mem]` and size compared).

#### F-17 Dead remnants of removed features are still compiled and tested
- **Where:** `src/util/car/healthview.{h,cpp}` (281+86 lines), `src/black/board/s3/mic_es8311.*` (60), `src/util/car/drivelog.cpp:24-124`, `src/util/car/fuelbook.cpp:83-140`, `carwire.cpp` `decode`, `platformio.ini:13-18`, `src/black/core/media.h:23-25`.
- **Evidence:** `cppcheck --enable=unusedFunction` lists 41 functions never used by firmware; `nm` of all three decks shows only `drivelog::appendRaw` linked, no `healthview`, no `fuelbook::parsePhrase`. `BACKLOG.md:86` says `healthview` "is kept" on purpose.
- **Fix:** decide per item (delete `mic_es8311` with F-14, the drive-log sheet helpers removed by #84, and the unused `[common]` embed list; keep `healthview` only if the new display deck is real).
- **Heap/flash risk of fix:** 0 B flash (already collected), -500 to -700 source lines. Needs a flash: no.

### MINOR: CI, release, tests, security

#### F-10 CI is not a required check
- **Where:** repo ruleset `protect-main` (via `gh api`), `.github/workflows/ci.yml`.
- **Evidence:** the ruleset has `deletion`, `non_fast_forward` and `pull_request` with `required_approving_review_count: 0` and no `required_status_checks`; `branches/main/protection` returns 404. Plan decision 7 listed this as not visible; `gh` shows it.
- **Fix:** add required checks for the three `Firmware (...)` jobs and `Host unit tests` (a ruleset edit, João's call).
- **Heap/flash risk of fix:** none. Needs a flash: no.

#### F-11 7 of 14 envs are never built in CI
- **Where:** `platformio.ini` (14 `[env:*]`), `.github/workflows/ci.yml:20-31`.
- **Evidence:** CI builds s3, hwcheck_s3, ledpanel, hwcheck, obddeck, obddeck_bench. Not built: blescan, bletest, ps3test, irtest, ledpanel_probe, s3_ota, obddeck_ota. All compile today (section 4). `ledpanel_probe` is the env `BACKLOG.md:36` tells the owner to flash.
- **Fix:** a `diag` matrix row (`-e blescan -e bletest -e ps3test -e irtest -e ledpanel_probe`) and the two `_ota` envs; about +1 to 2 min in parallel.
- **Heap/flash risk of fix:** none. Needs a flash: no.

#### F-12 The Actions cache is at the repository cap
- **Where:** `.github/workflows/ci.yml:41-47`.
- **Evidence:** `gh api repos/.../actions/cache/usage` = 10,483,922,978 B in 55 caches (97.6 % of 10 GiB). The key includes `hashFiles('platformio.ini')` with no `restore-keys`: 20 commits since `v2.0.0` each made a new ~190 MB cache per board.
- **Fix:** add `restore-keys: ${{ runner.os }}-pio-${{ matrix.board }}-`.
- **Heap/flash risk of fix:** none. Needs a flash: no.

#### F-13 No release since v2.0.0 and no binaries
- **Where:** `README.md:246-260`, `CHANGELOG.md:6`, `git tag`.
- **Evidence:** `git rev-list --count v2.0.0..origin/main` = 168 (feat 64, fix 50, docs 28, chore 9, `feat!` 2: #52, #57). The README rule gives MAJOR (v3.0.0). `gh release view v2.0.0`: 0 assets. `python tools/version.py` prints `2.0.0+168.g2a77658`; the boot screen shows `2.0.0+`, which hides how far past the tag a deck is. No device compares the `fw` field of `/state` and `/status` with its own version (skew between decks is silent inside `"v":1`). Pre-release tags and shallow clones give `0.0.0-dev`.
- **Fix (proposal only, nothing cut):** v3.0.0 from `[Unreleased]` when João wants; a `release.yml` on `v*` tags (build s3, ledpanel, obddeck; attach `.bin` and SHA-256; no media); show the commit count on the boot screen.
- **Heap/flash risk of fix:** 2 to 3 B of `.rodata` for the short string. Needs a flash: yes only to see it on screen.

#### F-20 Pillow-dependent Python tests skip silently in CI; CONTRIBUTING says the generators need only the standard library
- **Where:** `tools/test_gen_suki_ride.py:10-17`, `tools/test_gen_suki_hud.py:13`, `ci.yml:95-96`, `CONTRIBUTING.md:11,46`.
- **Evidence:** both suites `skipIf(g is None)`; CI never installs Pillow, so the `ART_X`/`ART_W` vs `sukiride.h` sync checks never run there. Four generators import PIL at module level.
- **Fix:** `pip install pillow` in the host-tests job; reword CONTRIBUTING.
- **Heap/flash risk of fix:** none. Needs a flash: no.

#### F-21 Parser and timer rules in CONTRIBUTING are not met in a few places
- **Where:** `src/util/car/nmea.cpp:21,105-106`, `test/test_nmea/test_nmea.cpp`, `src/util/common/jsonflat.cpp:142`, seven timer modules.
- **Evidence:** NMEA has an absurd-number cap and latitude/longitude limits but `grep -c "99999\|9100\|18100\|100000000" test_nmea.cpp` = 0. `jsonflat::numAt` ignores `strtol` ERANGE (a 20-digit number saturates and is accepted; `long` is 32-bit on the deck, 64-bit on the host). No wrap test in sprint, karaokemode, touchguard, carhealth, drivedash, obdpick, introclock (`introclock.h:20` also compares `soundAt >= t0`, not wrap-safe). A 26-suite keyword scan found no cut-off test in camwire, sigreq, markissue, tripbook, obdble, remotegate (heuristic: a zero means no matching text).
- **Fix:** one test per item; an `errno` check in `numAt`.
- **Heap/flash risk of fix:** a few bytes for `errno`. Needs a flash: no.

#### F-22 Four recurring tools have no tests; `version.py` downgrades pre-release tags
- **Where:** `tools/version.py:30-37`, `tools/check_flash.py`, `tools/ota_auth.py`, `tools/capture_serial.py`.
- **Evidence:** 24 of 44 non-test scripts have no test file. `version.from_describe("v2.1.0-rc1-3-gabc1234")` returns `0.0.0-dev`.
- **Fix:** `test_version.py` and a `check_flash` over/under test; accept pre-release tags or document that they are unsupported.
- **Heap/flash risk of fix:** none. Needs a flash: no.

#### F-23 SECURITY.md is out of date on three points
- **Where:** `SECURITY.md:69`, `:35`, no text for `MARKS_TOKEN`; `include/config.example.h:50-54`; `platformio.ini:165-166`.
- **Evidence:** it says 7 bundled roots, the bundle has 9 (`ca_bundle.inc` count `0x0009`); it says OTA on "both decks" though the yellow deck builds with `NO_WIFI_UPDATES`; `markpush.cpp:66` sends `Authorization: Bearer MARKS_TOKEN` to api.github.com and SECURITY.md never mentions that token.
- **Fix:** edit three sentences.
- **Heap/flash risk of fix:** none. Needs a flash: no.

#### F-24 Stale comments in `platformio.ini` and `ci.yml`
- **Where:** `platformio.ini:154`, `:259`, `ci.yml:30`.
- **Evidence:** `:154` says "~1.39 MB of the 1,966,080-byte slot"; the build says `Flash: 86.8% (used 1706901 bytes from 1966080 bytes)`. "one OTA slot" is ambiguous: `partitions_s3.csv` has two 3 MB slots.
- **Fix:** update or drop the numbers. Heap/flash risk: none. Needs a flash: no.

#### F-25 The host test runner shares one temp exe name per suite
- **Where:** `tools/ntest.ps1:27`.
- **Evidence:** the output is `GetTempPath()\ntest_<suite>.exe`; two runs at once (two worktrees, or the full run and one suite) write the same file.
- **Fix:** add `$PID` to the name. Heap/flash risk: none. Needs a flash: no.

#### F-26 CI hard-codes partition sizes that also live in the CSVs
- **Where:** `ci.yml:30,34,39,70-71`, `platformio.ini:260`.
- **Evidence:** `0x300000`, `0x1E0000`, `0x9E0000` typed in `ci.yml`, `3145728` in `platformio.ini`; today they match the CSVs (section 4), and `pack_media.partition()` already parses the CSV.
- **Fix:** let `check_flash.py` read the CSV, or comment the coupling. Heap/flash risk: none. Needs a flash: no.

#### F-27 CI hygiene: no job timeouts, cancel-in-progress on main, libs re-downloaded
- **Where:** `ci.yml:12-14,36,72`.
- **Evidence:** `grep -n timeout-minutes ci.yml` is empty (default 360 min); `cancel-in-progress: true` also applies to pushes to main; `pip install platformio==6.2.0` pins only the top package; Unity (37 s step) is re-downloaded per run.
- **Fix:** `timeout-minutes: 15`; `cancel-in-progress: ${{ github.ref != 'refs/heads/main' }}`; cache `.pio/libdeps/native`.
- **Heap/flash risk of fix:** none. Needs a flash: no.

#### F-28 The two VT323 font sets and wrappers are copies with no test tying them
- **Where:** `src/black/fonts/vt323_*.h` vs `src/yellow/fonts/vt323_*.h`, `src/util/black/lyricface.*` vs `src/yellow/ui/termfont.*`.
- **Evidence:** `diff` of each pair shows 16 lines (comment, `#include`, namespace); regenerating with `tools/gfxfont.py` reproduces all eight files. So they are generated twins, **not drift today**. `grep -rn "vt323\|gfxfont" test` finds no test comparing them; `lyricface.cpp:2-5` includes `../../black/fonts/` from `util/`.
- **Fix:** one font directory under `src/common/` with thin wrappers, or a host test that hashes both sets.
- **Heap/flash risk of fix:** none. Needs a flash: no.

#### F-29 Black s3 links `httpget.cpp` only because `--gc-sections` drops it
- **Where:** `platformio.ini:304-305` vs `:228-229`; `src/common/net/httpget.cpp:4` includes yellow-only `util/yellow/httphead.h`.
- **Evidence:** s3 compiles `common/net/httpget.cpp` but not `httphead.cpp`; `nm -C .pio/build/s3/firmware.elf | grep -iE "httphead|httpGet"` is empty. The link passes by accident.
- **Fix:** add `-<common/net/httpget.cpp>` to s3's filter, as obddeck does. Heap/flash risk: none. Needs a flash: no.

### MINOR: UX and process

#### F-18 Cockpit alert contrast is thin on the default green and yellow night themes
- **Where:** `src/util/common/termpal.cpp:47-58`, `src/black/ui/cockpitview.cpp:126-139`.
- **Evidence:** a script over RGB565-quantised palette values (WCAG, Machado 2009 colour-blind simulation): green night TEXT vs ALERT 3.00:1, MID 2.34, DIM 1.63; deutan dE76 27.0 / 18.6 / 14.0. Yellow night TEXT vs ALERT 2.27 (deutan 26.6). Blue night TEXT 3.3, MID 2.8, DIM 2.2 on black. The alert is an outline plus red type, so it is not colour-only; the lit-vs-hot tick colours rely on position.
- **Fix:** keep the outline; make ALERT bolder (inverted or 2 px) or warm the red toward 0xFF5A5A (deutan dE > 40); lift blue DIM to >= 3:1.
- **Heap/flash risk of fix:** none (constants). Needs a flash: yes (the owner picks these by eye on the panel).

#### F-19 The smallest faces are below an in-vehicle minimum and carry red link messages
- **Where:** `src/yellow/ui/termfont.h:23-33` (17 px: `>DECK: NO LINK`, `>GPS: NO FIX`, `>LINK 2/2`), `src/black/ui/cockpitview.cpp` (labels, tick digits at 19 px).
- **Evidence:** cap height of H is 10 px (17), 11 px (19), 14 px (24), 21 px (36). Assuming a 57.6 mm active width on a 2.8" 320 x 240 panel and 60 cm viewing, 10 px is ~10 arcmin against ISO 15008's 16 arcmin (quoted from memory); only the 36 px face passes. Viewing distance and panel size are assumptions.
- **Fix:** keep link and alert wording out of the 17 px face; reserve 17 and 19 px for non-critical labels.
- **Heap/flash risk of fix:** none (picks an existing face; the longest message must still fit 320 px). Needs a flash: yes.

#### F-30 Phone page: the five screen buttons overflow phones 390 px wide or narrower
- **Where:** `src/black/net/remote.cpp:30-65`.
- **Evidence:** headless Edge: min-content row 371 px; `scrollWidth` exceeds the client width at 320, 375 and 390 px; COCKPIT's right edge lands at 391 px, clipped 16 px at 375 px. Touch targets are 57 px high (fine). Title, artist and clip names use `textContent`, not `innerHTML`. Not measured on a phone font.
- **Fix:** `#sc{flex-wrap:wrap}` or `flex:1 1 30%`.
- **Heap/flash risk of fix:** ~20 B of flash. Needs a flash: yes (to see it on a phone).

#### F-31 "Pick COCKPIT while parked" fails open when the link is lost
- **Where:** `src/util/car/drivedash.cpp:30`, `drivedash.h:17-19`, `src/black/car/gauges.cpp:112-120`, `remote.cpp:268-269`.
- **Evidence:** `canSwitch(kmh) = !(kmh > MOVING_KMH)`, `MOVING_KMH 3.0`, "NAN: parked". The check is in `gauges::setScreen`, shared by the phone page (409), `/gauges?screen=` and the serial command, so it is enforced in code. With the adapter lost (speed NaN) a moving car can switch; GAUGES on/off and `/fill` have no moving gate.
- **Fix:** document the NaN case, or use the last good speed for 10 s. Heap/flash risk: none. Needs a flash: no.

#### F-32 The yellow deck has no motion gate on touch
- **Where:** `src/yellow/ledpanel.cpp` (no speed source), `docs/yellow-deck.md:46-113`.
- **Evidence:** since #52 the deck no longer polls `/car`; `touchguard`, `tapguard`, `padguard` protect against false presses, not against use while moving. Harm is limited to lights, karaoke and music.
- **Fix:** state in README that touch is for parked use and the pad and remote for driving. Heap/flash risk: none. Needs a flash: no.

#### F-33 Design records: 25 of 63 not indexed, 8 stale status lines, 10 obsolete files
- **Where:** `docs/superpowers/README.md`, `docs/superpowers/{plans,specs}`.
- **Evidence:** 31 plans + 32 specs (the plan said 57); the index stops at 2026-10-31; 27 files are not named in it (two are covered by a `boot-scene*` row). Classification: live 27, historical 24, obsolete 10 (companion-screen, avatar, avatar-home, music-mode, car-health-deck and car-health-check, plan and spec each), parked 2 (yellow-horn, voice-agent-next). Stale `Status:` headers include black-deck-karaoke ("not built", merged as #55).
- **Fix:** add the missing rows with a Status column, fix the eight headers, move the ten obsolete files to `docs/superpowers/archive/` (renames only; nothing deleted).
- **Heap/flash risk of fix:** none. Needs a flash: no.

#### F-34 30 remote branches are not ancestors of main; 28 look squash-merged
- **Where:** `git branch -r --no-merged origin/main`.
- **Evidence:** 30 listed of 32 remote branches; 28 match a merged PR by title (#68 to #95); only `feat/voice-agent` and `claude/intro-greetings-8b8b67` are parked. Equivalence was matched by title, not per branch content. `git stash list` is empty.
- **Fix:** João deletes the 28 PR branches after a glance (`gh pr list --state merged --json headRefName`). Nothing was deleted. Heap/flash risk: none. Needs a flash: no.

#### F-35 CHANGELOG lacks #92, #93, #94, and has no PR numbers
- **Where:** `CHANGELOG.md:6-278`.
- **Evidence:** `grep -n "press age\|wait for the loop\|padedges" CHANGELOG.md` has no hit; pink appears only as a side remark at `:16`. `[Unreleased]` is 272 lines and 65 bullets.
- **Fix:** add the three entries and PR numbers. Heap/flash risk: none. Needs a flash: no.

#### F-36 Agent kit cites 18 private memory notes
- **Where:** `.claude/skills/esp32-dev/references/*.md`, `.claude/agents/esp32-reviewer.md:35`.
- **Evidence:** `grep -rn "memory \`" .claude CLAUDE.md docs README.md` = 18 hits; the notes live outside the repo, so a fresh clone cannot resolve them. Accuracy checks passed: all 14 envs, partition offsets and port wording match `platformio.ini` and `partitions_s3.csv`.
- **Fix:** inline the one-line fact where a note is cited. Heap/flash risk: none. Needs a flash: no.

### NIT

- **N-01** `reqauth::pass` keeps 24 static `String`s (288 B DRAM) and a few small long-lived blocks (`src/common/net/reqauth.cpp:29`); borrow from `WebServer::arg(i)` instead.
- **N-02** `mem::bigAlloc` falls back to internal `malloc` silently when PSRAM is out (`src/black/core/mem.cpp:47-50`); log once.
- **N-03** Stale build config: unused `[common] embed_files` (`platformio.ini:13-18`), seven empty `embed_files =` overrides, `pre:` clip scripts on every `cyd` env, legacy `COMPANION_DECK` (`remote_config.h:32`), ARCHITECTURE `:437` omits `irtest`.
- **N-04** `clockText` is defined twice (`gaugeview.cpp:109`, `sukiview.cpp:53`); `shotRow` twice (`serialcmd.cpp:11-29`, `ledpanel.cpp:3236-3257`).
- **N-05** Black tasks outside `net` and the loop are not watched by the task watchdog (`camsound.cpp:190`, `greeting.cpp:182,232`, `sink_es8311.cpp` with `portMAX_DELAY` locks); a stuck I2S write would silence camera alerts with no reboot.
- **N-06** Yellow keeps TFT_eSPI's smooth-font path (`SMOOTH_FONT=1`, `platformio.ini:50`) that nothing calls: `nm` retains ~1.3 KB, not the 16.8 KB quoted for the S3 (`:317`).
- **N-07** No `.gitattributes` and no `end_of_line` in `.editorconfig`; `git ls-files --eol` shows 736 files `i/lf w/crlf` because of local `autocrlf`.
- **N-08** Bearer-key routes have no failed-attempt throttle and `config.example.h:33` gives no minimum token length; `tools/spotify_auth.py` has no OAuth `state` (loopback only).
- **N-09** Serial `R-5` accepts a negative row (`serialcmd.cpp:63`, `ledpanel.cpp:3332`); debug-only, needs USB.
- **N-10** `platformio.ini` repeats pins (TFT_eSPI x4, NimBLE x3, XPT2046 x2, PS3 x2, IRremote x2) and NimBLE role flags x2; hoist into fragments and diff `pio project config` before and after.
- **N-11** `ota_auth.py:17` puts the OTA password on the espota command line (`--auth=`); `--debug` is correctly removed; low risk on a one-user PC.
- **N-12** Small doc drifts: CONTRIBUTING layout omits `src/obd/` and `util/car/`; README "CI builds both decks"; CREDITS cites removed `data/jp16.bin` and `tools/gen_jp_font.py`; `.claude/worktrees/` ignored only through `.git/info/exclude`; `.gitignore` lists old worktree folder names.
- **N-13** No test or note ties the two pink constants (`src/black/ui/pink.h:7`, 0xFF00B4, vs ledlook 0xFF0090); by design tuned by eye (5.92:1 and 5.68:1 on black, dE 17.9 apart).
- **N-14** `introclock.h:20` compares `soundAt >= t0` (not wrap-safe); fold into F-21.

## 3. Phase results

### 3.1 Memory (Phase 2), static only

Per-env budget, from this session's builds (`pio run -e <env>`, size line of the log):

| Env | Flash used | % of app slot | RAM (static) | % of 327,680 B |
|---|---|---|---|---|
| s3 (slot 0x300000) | 2,362,833 B | 75.1 % | 76,248 B | 23.3 % |
| ledpanel (slot 0x1E0000) | 1,706,901 B | 86.8 % | 64,640 B | 19.7 % |
| obddeck (slot 0x1E0000) | 1,179,689 B | 60.0 % | 69,172 B | 21.1 % |
| obddeck_bench | 889,421 B | 45.2 % | 55,868 B | 17.0 % |
| s3_ota / obddeck_ota | 2,362,881 / 1,179,737 B | 75.1 / 60.0 % | 76,248 / 69,172 B | 23.3 / 21.1 % |
| hwcheck / hwcheck_s3 | 315,225 / 343,485 B | 10.0 / 10.9 % | 22,404 / 20,132 B | 6.8 / 6.1 % |
| blescan / bletest | 605,485 / 653,197 B | 19.2 / 20.8 % | 35,900 / 36,348 B | 11.0 / 11.1 % |
| ps3test / irtest | 1,073,753 / 335,097 B | 34.1 / 10.7 % | 39,164 / 23,332 B | 12.0 / 7.1 % |

`xtensa-esp32-elf-size` (text / data / bss): s3 1,213,386 / 1,165,712 / 1,235,609 (the large data is the embedded 738 KB Ride art and flash-mapped fonts and sprites); ledpanel 1,472,200 / 256,984 / 37,001; obddeck 1,012,960 / 189,012 / 41,697. Media pack: the 95 % gate uses 0x9E0000; this review did not pack media (rule 1). The yellow deck sits 200 KB under CI's 97 % gate, the closest of the three to its limit. Largest functions are libc `printf`/`scanf` families (0x2ecb to 0x2fe2 B) and, on s3, `car_sprite::CAR_PX` (0x6720 B) and `dex::DEX` (0x2008 B); no application symbol is out of proportion.

Allocation hygiene (118 grep lines, 32 `String` lines): every site inside a loop or per request is bounded; the per-request `String`s (`remote.cpp`, `lrclib.cpp`, `walksprite.cpp`) are freed on return; two sites matter for TLS (F-07, F-15); no `std::function`, `shared_ptr`, iostream or exception anywhere in `src/`. Tasks (19 `xTaskCreate*` plus the loop tasks): no local over 512 B on a small stack; the largest are `char line[512]` on the 10 KB net stack and `Item items[96]` (~1.5 KB) on the 8 KB black loop. The yellow deck watches all four of its tasks and the OBD deck its three; black watches `net` and the loop only (N-05). Stack high-water marks: not measured. TLS users (black only; the yellow and OBD decks have no `WiFiClientSecure`): Spotify API (kept open), Spotify token (**no gate**, F-07), LRCLIB, Apple, sprite sheets, camera marks (gated), album art (plain HTTP). `mem::CAPS` is internal 8-bit, so the largest block is read where TLS allocates.

Reclaim opportunities (the plan asked for at least 5 with bytes and risk):

| # | Item | Saves | Risk | Finding |
|---|---|---|---|---|
| 1 | `s_api.stop()` before the hourly `login()` | ~20 KB internal at the refresh instant | low | F-07 |
| 2 | Black TX-only I2S | ~4.1 KB internal per clip + 20 s | low to medium | F-14 |
| 3 | Yellow music task at boot or gated | 0 B net; removes a 4.4 KB surprise | low | F-05 |
| 4 | `snprintf` token body | ~300 B transient | low | F-15 |
| 5 | Unflag `SMOOTH_FONT` on ledpanel | ~1.3 KB flash (measured by `nm`) | low | N-06 |
| 6 | `reqauth` borrowed params | 288 B DRAM | low | N-01 |
| 7 | Dead code (`healthview`, `mic_es8311`, drive-log helpers) | 0 B flash (already collected), -500 to -700 lines | low | F-17 |
| 8 | Trim yellow task stacks (voice, music, leds-link 4096; deck 5120) | 1 KB each per 1 KB trimmed | medium; needs `[mem] stack left:` from a loaded run | not measured |
| 9 | Drop the second JPEG decoder on s3 (tjpgd 3.3 KB vs JPEGDEC 16.3 KB) | <= 3.3 KB flash | high (speed) | not recommended |

Yellow worst case (code and docs only): idle ~37 KB free with low water ~30 KB; fixed costs are four task stacks (17,408 B with music) + the 6,144 B loop + TCBs, WiFi buffers capped at 12+12 with AMPDU off, no TLS. Protected by heapguard: only the deck task's asks. Not protected: music task creation (F-05), voice task connections and POST bodies (F-08), `deckSearch`, NVS saves. The margin between the guard and the observed failure level is about one ask (F-06). Whether it holds in the car is **not measured**.

### 3.2 Architecture and coherence (Phase 1 mechanical + Phase 3 judgment)

- **Layering:** `util/` has no Arduino, WiFi, `ui/`, freertos or `esp_` includes. Exceptions, both header-level: `util/black/lyricface.cpp:2-5` includes `black/fonts/`, `util/lights/ledintro.cpp:2` includes `yellow/elkcmd.h`; and `common/net/httpget.cpp:4` includes `util/yellow/httphead.h` (F-29). `black/car/gauges.cpp:12` includes `obd/obdlink.h` by design under `OBD_LINK`. `black/core` and `images` never include `ui/`. Black never includes yellow or the reverse.
- **Coverage of the allow-list:** 180 `.cpp` files, 0 compiled by no env, 0 headers with no includer. `.cpp` per env: s3 156, obddeck and obddeck_bench 54, ledpanel 38, hwcheck_s3 6, the other diag envs 1. 19 files are shared by s3 and ledpanel (`common/`, `util/common|wire|lights`).
- **Cross-deck contracts:** signing (`sigreq`+`hmac256`: 4 consumers, tested), music UDP palettes (`musicwire` v2), `/state` (`statejson`), `/car` and `/obd` (`"v":1`), all have a shared definition and a suite. The query strings for `/music /karaoke /link /playlists/play` are string literals on both ends with no test tying them (`ledpanel.cpp:2335,2406,2426,2457` and `remote.cpp:202-326`). Auth: `reqauth::pass` (key or signature) on most routes; `/play`, `/trip/reset`, `/cam/*` use bare `keyOk` (bearer key only, not signable); the OBD listener is signed-only.
- **Duplicated concepts, black vs yellow:** light looks (`ledlook`, single), terminal palette (`termpal`, single), day and night (`daynight`, single), deck link (`decklink`, single): all tied by tests. Pink: two constants on purpose (N-13). VT323: generated twins (F-28). `clockText`, `shotRow`: duplicated (N-04). `/fill` parsing: drifted (F-09).
- **Unit size:** NLOC 28,569, 1,896 functions, 97 with CCN > 15, 4 over 100 lines (F-16); files over 500 lines: `ledpanel.cpp` 3,493, `main.cpp` 735, `obdlink.cpp` 660 to 705, `bootscene.cpp` 672 to 696, `sukiride.cpp` 667.
- **Concurrency (read):** black's `net` task is the only writer of `AppState` (via `shared::publish`), the UI loop and HTTP task read snapshots; audio holds `s_lock` across a 400 ms settle in `open`. Yellow: the UI loop owns state, the voice task hands a job through a 1-slot queue, the deck task holds `deckLock` across the network call by design. 49 `portENTER_CRITICAL` sites, 25 `volatile`, 29 `std::atomic`. Callbacks only set 32-bit flags.

Failure and recovery:

| Failure | Handling | Where |
|---|---|---|
| A watched task hangs | 90 s task watchdog reset; black watches `net` + loop only (N-05) | `common/core/watchdog.h:23` |
| WiFi lost | fast-failing polls, rejoin back-off 5/15/30/60 s; black restarts after 15 min (**F-02**) | `nettask.cpp:261-285`, `netup.h:42` |
| Spotify 429 | `PollBackoff`, no reboot | `nettask.cpp:274` |
| Heap starved (black) | optional steps deferred; restart after 180 s only with memory evidence | `nettask.cpp:305`, `netplan.h:122-129` |
| Heap low (yellow) | asks wait (heapguard), WiFi buffers capped | `ledpanel.cpp:2268`, `wifibufs.cpp` |
| Adapter lost | reconnect back-off 5 s to 60 s, Cockpit `NO LINK` / `KEY ON` pill | `obdlink.cpp:28,541-556` |
| Bad firmware | boot guard rolls back after 3 crash resets; `ota::onOnline` confirms after a minute | `ota.cpp:129-156` |
| Crash | `crashlog` keeps 6 runs in NVS | `crashlog.cpp:26-54` |
| NVS full / empty media partition | camera mark returns 507; features skip, `[media] none` | `remote.cpp:367`, `media.cpp:20` |
| OBD `/car` consumer missing | nothing in the repo reads it (F-09) | `src/obd/main.cpp:5` |

### 3.3 to 3.6 Standards, config, hygiene (Phase 1)

- **C++:** `strcpy` 5 (all size-safe), `strcat`/`sprintf`/`gets`/`strtok`/`atof` 0, `atoi` 2 (debug only, N-09), exceptions/RTTI 0, `std::function`/`shared_ptr`/iostream 0. cppcheck 2.21.0 (`--enable=warning,performance,portability`, three define sets): `subtractPointers` 25 (embed macro), `uninitMemberVarNoCtor` 33, `uninitvar` 4 (each read: false positive), 2 `arrayIndexOutOfBoundsCond`, 2 old-style casts, performance and portability 0. **No real defect.** `ledpanel.cpp` returned no output (cppcheck lacks the Arduino and Bluedroid headers), so it was not analysed that way. `#if 0`, `TODO`, `FIXME`, `HACK`: 0 (the two `XXX` hits are `\uXXXX` comments). Not produced: the extended `-Wconversion` family warning counts (they need a build with changed flags; not done to keep `.pio` unchanged), clang-tidy (not installed).
- **Python** (scratch venv, 64 files, 6,986 lines): ruff `E,F,W,B,UP,S,SIM` 1,252 findings, of which 819 are E501 (no line length configured), 205 UP031, 128 S101 (120 in tests); F-class 0; `F,B` only 13 (9 `zip` without `strict`, 4 unused loop variables). mypy 36 errors in 14 files, all noise (PlatformIO injected names, test fallbacks, `os.sys`). bandit `-ll`: 13 Medium (all `urlopen` on fixed https URLs), 0 High. vulture at 80 %: 1 hit. No `shell=True`, `eval`, `exec`, `pickle`, `os.system`. Tested vs untested: 20 of 44 non-test scripts have a test file; 24 do not (rated in F-22). `unittest discover`: 126 tests OK locally (Pillow present); CI skips two suites (F-20).
- **PowerShell:** read only (PSScriptAnalyzer not installed). The include scraper follows quoted includes, over-approximates (a miss is a loud link failure, never a silent pass); all 140 suites have exactly one `.cpp`; `$IsLinux` is undefined on Windows PowerShell 5.1 but falsy; sanitizers run on Linux only (CI).
- **Config:** every `extends` target exists; the claim that `min_spiffs.csv` and `huge_app.csv` share NVS 0x9000/0x5000 and otadata 0xE000 with the pinned platform is **true**; `partitions_s3.csv` has 4 KB alignment, no overlap, no gap, and ends exactly at 0x1000000. Pins are SHA-pinned for actions (verified against the tag commits), version-pinned for libs; `pio pkg outdated` was not run (rule 1), so "outdated" is not measured.
- **Generated data:** `icon.inc`, `car_sprite.inc`, `gojira16.h` and the eight `vt323_*.h` regenerate byte-identically in a scratch copy; `ca_bundle.inc`, `dex_data.inc`, `status_sprite.inc`, `genre_data.inc` need live network data and were not regenerated.
- **Docs and links:** 102 Markdown files; 76 relative links, 0 broken. Backtick path misses in live docs: `data/drive.snd` and `data/rosters.txt` (git-ignored by design), `tools/micrec_pull.py` (planned), two removed files in CREDITS. `actionlint` not installed; `ci.yml` read by hand: `permissions: contents: read`, `persist-credentials: false`, SHA pins, no `pull_request_target`, no secrets in `run:` interpolation.

### 3.7 Performance

Estimates from code reading only, nothing measured. The OBD polling budget (`util/car/obdplan.cpp`, 6 requests/s; #90's 3 s engine-load turn costs RPM and speed 8 to 12 % of refresh per the repo's own skill note) was not replayed with `tools/obd_sim.py` (it opens no device, but it was outside the time spent). Network: Spotify polling uses back-off (`PollBackoff`, `LoginBackoff`) and a kept-open session; the only per-hour spike is F-07. Display: black intro runs at 20 fps by design (~50 ms/frame per BACKLOG), per-screen sprites are 153,600 B in PSRAM (allocated once); no per-frame `String` found in `black/ui`. Boot times and touch/PS3 latency: **not measured**.

### 3.8 Versioning and 3.9 CI/CD

See F-10 to F-13, F-20 to F-22, F-24 to F-27. CI gap table:

| Item | Present? | Cost to add |
|---|---|---|
| Build s3, ledpanel, obddeck, hwcheck, hwcheck_s3, obddeck_bench | yes | n/a |
| Build the other 7 envs | no | +1 to 2 min parallel |
| Host tests, 64-bit ASan/UBSan and 32-bit | yes (140 suites, 3.9 to 5.0 min) | n/a |
| Python `unittest` | yes, Pillow suites skip | `pip install pillow` ~1 s |
| Flash % (97 %) and media (95 %) gates | yes | n/a |
| RAM % gate | no (ledpanel 19.7 %, obddeck 21.1 %, s3 23.3 % today) | 3-line log parse |
| Binary-size delta vs main | no | ~20 lines |
| `.elf`/`.map` artefacts | no | `upload-artifact`, ~5 MB per run |
| Static analysis, Python lint, actionlint, link check | no (cppcheck clean; ruff F,B = 13 non-bugs; 0 broken links, so each gate would pass today) | minutes each |
| Secret scan | GitHub secret scanning and push protection on; no gitleaks | optional |
| Required status checks | no (F-10) | one ruleset edit |
| Job timeouts, cache `restore-keys` | no | 2 lines each |
| Workflow security (read-only token, SHA pins, no fork secrets) | yes | n/a |

Last 20 CI runs: 3.6 to 5.5 min each (median ~4.5), all `success`; the latest main run: host tests 3.9 min, s3 1.6, obddeck 1.4, ledpanel 1.1. Dependabot covers `github-actions` only (PlatformIO libraries are not supported by it). Test quality: every one of the 121 `src/util` modules is `#include`d by at least one suite (the ARCHITECTURE claim holds; inclusion is not coverage and **line coverage was not measured**). 70 of 101 modules outside `util/` (16,033 of 21,055 lines) have no suite; the ones with decision logic are `obd/carstate.cpp`, `black/core/nettask.cpp` and `common/net/*`.

### 3.10 UX

**Screen inventory.** Black: Now playing, status screens (No WiFi, No internet, Spotify busy/error, Certificate, Login failed, Waiting), karaoke, update screen, boot opening and montage, GAUGES (Suki or the street look "CITY DATALINK", Drive, Trip, Ride, Cockpit), camera sign and band, phone page (screen buttons, FULL TANK + price, Howl, clips), watch playlist picker; inputs are the phone page, the watch and serial desk commands (no touch). Yellow: boot, main screen (header, GPS and DECK tags, PAD battery, clock/trip, lights panel, KARAOKE and MUSIC switches, prompt line), Lights (BOTH/INSIDE/OUTSIDE/PRESETS tabs, 2 x 4 colour tiles, level bar), touch calibration, PS3 pad, IR remote (JUMP/FADE keys dead while `LED_EFFECTS 0`), `/say`, watch. Watch: pages BOTH, INSIDE, OUTSIDE, PRESETS, PLAYLISTS; START opens a list; menu depth 2.

**Contrast and colour-blind table** (script on RGB565-quantised values; WCAG ratio on black; dE76 under Machado 2009 severity 1.0). Night `termpal`, what both decks draw at any hour:

| Theme | TEXT | MID | DIM | ALERT | TEXT vs ALERT | dE none / protan / deutan |
|---|---|---|---|---|---|---|
| red | 5.3 | 4.4 | 3.2 | white | 4.00 | 115 / 76 / 76 |
| orange | 6.1 | 4.9 | 3.5 | white | 3.44 | 106 / 75 / 76 |
| pink | 5.3 | 4.5 | 3.2 | white | 3.94 | 102 / 62 / 62 |
| purple | 5.3 | 4.3 | 3.1 | red | 1.01 | 140 / 127 / 132 |
| blue | 3.3 | 2.8 | 2.2 | red | 1.59 | 164 / 133 / 161 |
| green (default) | 15.7 | 12.3 | 8.6 | red | 3.00 | 160 / 65 / **27** |
| white | 21 | 17.8 | 14.1 | red | 4.00 | 115 / 76 / 76 |
| yellow | 11.9 | 11.9 | 11.9 | red | 2.27 | 71 / 45 / **27** |
| cyan | 13.9 | 10.7 | 7.2 | red | 2.64 | 152 / 81 / 96 |

TEXT vs MID is 1.18 to 1.30:1 in every theme (yellow: identical), so the three levels differ by hue and position, not contrast. The `termpal.h` header figures match the script except the RGB565 rounding. Other palettes: `theme.h` day text on box 9.73, night text 9.93; HP green/yellow/red on empty 7.16 / 8.90 / 4.35 (bar length also encodes value); the street look's DIM on BG is 2.44 and the Trip rules 1.96 (both decorative lines). Swatch grid under protan: purple/blue dE 3.3, green/yellow 8.6, red/orange 18.7; every tile carries its name and the selection is an inverted bar plus ring, so colour is not the only cue.

**Black vs yellow divergence:**

| Aspect | Yellow | Black | Verdict |
|---|---|---|---|
| Palette | `termpal` night per light theme | Cockpit and karaoke share it (#89, #91); Trip, Drive and the street look use fixed palettes; Ride follows the inside light (#82) | Trip/Drive/street: drift from the rule, undocumented as intent |
| Pink | `ledlook` 0xFF0090 | `PINK_RGB` 0xFF00B4, tuned by eye on a BGR/IPS panel (#94) | intentional |
| Background | pure black | Cockpit and karaoke black; Trip 0x020A04; Drive and street tinted | drift on Trip/Drive/street |
| Type | VT323 17/19/24/36 | the same tables via `lyricface`; other screens use TFT_eSPI fonts and segment digits | Cockpit and karaoke match |
| Alert | red or white type, outline | Cockpit: outline + red type (matches); street: filled WARN_BG box | street = drift ("no fill" rule) |
| Rules | 1 px hairlines | Cockpit 1 px, ticks 3 px | matches |

**Interaction while moving:** phone page, `/gauges?screen=` and the serial command share the 3 km/h gate (NaN counts as parked: F-31); GAUGES on/off and FULL TANK (a `confirm()`) have no gate; yellow touch has only stuck-press (5 s) and 250 ms post-change guards (F-32); the PS3 pad has `padguard` (quarter travel, 3+ buttons ignored, 0.5 s after set-down, 5 s stuck) and #92 queues presses instead of dropping them; IR accepts NEC address 0xEF00 only. **Feedback on faults:** WiFi, Spotify, certificate and login failures draw status screens with a reason and a countdown; adapter lost shows the Cockpit `NO LINK` / `KEY ON` pill; deck link shows `>DECK: NO LINK` (17 px, F-19); a low-heap or WiFi-down restart shows nothing (F-02). Legibility judged by eye at night, flicker and brightness: **not measured**.

### 3.11 Security and privacy

- **Secrets:** gitleaks is not installed; the substitute was `git log --all -p -G` over token patterns (GitHub, AWS, private-key headers, `client_secret`, `Bearer`) and over `#define` secret names with 4+ character values. Result: 0 real hits; two commits show the placeholder `"your_client_secret"` in docs. `.gitignore` covers `src/config.h`, `Secrets.mc` and the personal media (checked with `git check-ignore -v`); no plate string in tracked text; GitHub secret scanning and push protection are enabled.
- **Listener table:**

| Listener | Auth | Replay / abuse | Notes |
|---|---|---|---|
| Black WebServer :80 (off if `REMOTE_TOKEN` empty): `/`, `/icon.png` | none (no data) | n/a | by design |
| Black `/state /music /karaoke /link /obd /gauges /fill /playlists /playlists/play /cam /cam/mute /cam/mark` | `reqauth::pass`: bearer `k=` or HMAC `t,n,sig` | signed: 120 s window, 32-nonce ring, constant-time compare; bearer: none | several state-changing GETs; the bearer key crosses the hotspot in clear (documented) |
| Black `/play`, `/music/debug`, `/cam/list`, `/cam/marks`, `/trip/reset`, `/cam/marks/clear` | bearer key only | `Gate::request` (409, 429) on `/play` only | destructive routes use the bearer path (F-08, N-08) |
| Yellow :80 `/say` (ANY), `/state`, `/presets`, `/status` | `reqauth::pass` | as above | `/say` accepts POST (F-08) |
| OBD :80 `/car`, `/fill` | signed only (bearer refused) | signed window and ring | strictest listener |
| ArduinoOTA :3232 | `OTA_PASSWORD` challenge-response | boot guard rollback after 3 crash resets; image not signed (documented) | black and OBD only; yellow is `NO_WIFI_UPDATES` |
| Music UDP :4210 | 8-byte HMAC tag, boot id, rising counter | counter, boot id | tested |

TLS clients: Spotify and GitHub use the 9-root bundle; lyrics, Apple, sprite and art fetches use `setInsecure()` (public data; no credential goes through them, checked in `lrclib.cpp` and `apple.cpp`); no token is ever logged.
- **Parsers:** NMEA, Spotify JSON, LRC, Apple, camera CSV, wire formats and ELM327 all have bounded buffers and a suite; gaps are in F-21 (NMEA absurd number, `numAt` ERANGE, several cut-off tests). No `%s` `sscanf`, `strtok` or `gets`.
- **Repo settings (readable with `gh`):** ruleset `protect-main` (F-10), secret scanning on, `default_workflow_permissions: read`. Settings that `gh` cannot show (reviewer rules for forks and others) are **not verified**.

### 3.12 Process, docs, agent kit

**Calibration set #90 to #94:**

| PR | Tests | CHANGELOG | Other docs | Car check in BACKLOG |
|---|---|---|---|---|
| #90 load every 3 s | yes (`test_obdplan` +45) | yes | polling-budget note, spec | no |
| #91 Cockpit look | yes (`test_cockpit`) | yes | ARCHITECTURE, spec | no |
| #92 PS3 presses wait | yes (`test_padedges` +65) | **no** | `yellow-deck.md` | no |
| #93 black pink | no | **no** | none | no |
| #94 pink 0xFF00B4 | no | **no** | none | no |

The review's dimensions would have caught the missing CHANGELOG lines and BACKLOG tracking (this section) and the contrast point (F-18); the memory and flash dimensions have nothing to say about #93 and #94. The #86 Ride-art guard is tested (`tools/test_media_sizes.py:20-24`).

Docs drift list: ARCHITECTURE intro, Layout and Ride (F-04); README gauges (F-04); BACKLOG (F-03, `ledpanel.cpp` size 3,493 vs ~2,800, CI matrix list); CHANGELOG (F-35); `docs/superpowers` index (F-33); CREDITS (N-12); CONTRIBUTING layout and "standard library only" (F-20, N-12); 18 private-note references (F-36); the review plan's own figures (63 design records, not 57; 20 Python test files, not 14; `termpal` is in `util/common/`, not `util/yellow/`).

`docs/review` loop closure. 2026-11-15 audit (15 rows): 8 done or dropped; 5 open (row 1 `LED_EFFECTS 0` still the default, row 4 camera bench, row 5 car checks #47 #55 #57 #58 #62, row 14 diagnostics not in CI now 7 envs, row 2 voice agent parked). 2026-10-08 review: done `.editorconfig`, `ntest.ps1`, `spotify_auth.py`, `requirements.txt`, strict `-W` flags, CI; open `.clang-format` (absent), `pio check` (not in CI), one `setInsecure` family left (a CA bundle exists). The older `-e esp32dev` references in that file are historical.

The reviewer self-test (`esp32-reviewer` on #91 and #92, `obd2-reviewer` on #90) was **not run**; the agent kit's accuracy against today's envs, ports and offsets was checked by reading and passed.

## 4. Metrics baseline (for the next review to diff)

| Metric | Value (2026-11-24, `2a77658`) |
|---|---|
| Envs building | 13 of 13 attempted pass (`s3`, `ledpanel`, `obddeck`, `hwcheck`, `hwcheck_s3`, `obddeck_bench`, `blescan`, `bletest`, `ps3test`, `irtest`, `ledpanel_probe`, `s3_ota`, `obddeck_ota`); `native` is the host-test env. Build seconds (cold-ish): s3 73, ledpanel 104, obddeck 284, hwcheck 111, hwcheck_s3 213, obddeck_bench 252, blescan 121, bletest 162, ps3test 66, irtest 76, ledpanel_probe 201 |
| App flash / RAM | table in section 3.1 |
| Library warnings in the 3 diag builds | PS3 Controller Host `-Wdiscarded-qualifiers`, IRremote `-Wpragmas`/`-Wcpp`: all in `.pio/libdeps`, outside the `-Werror` scope |
| Host suites | 140 of 140 pass, 64-bit with ASan/UBSan, 20 min wall clock on this PC (1,254 s) |
| 32-bit host run | see section 5 note (the local toolchain has no 32-bit `libstdc++`); CI covers it |
| Python tests | 126 OK locally with Pillow; CI skips 2 suites |
| Source size | `src/` ~40 K lines C/C++, 180 `.cpp`, NLOC 28,569, 1,896 functions, 97 with CCN > 15; `tools/` 64 `.py`, 6,986 lines; 121 util modules |
| Commits since tag | 168 since `v2.0.0` (feat 64, fix 50) |
| CI | median ~4.5 min per run; Actions cache 10.48 GB of 10 GiB, 55 caches |
| Runtime: free heap min, largest block min, PSRAM, stack HWM, allocation failures, resets per capture, TLS failures | **not measured** (Phase 5 not run) |
| Line coverage of `util/` | **not measured** |
| Extended `-Wconversion` family counts | **not produced** |

## 5. Follow-up PR proposals (not opened; João picks)

Each is independent unless noted. "Flash" = needs a deck to verify. None adds a permanent tool, config or gate to the repo until chosen (decision 2).

| # | PR | Scope and files | Evidence | Saves | Risk | Flash |
|---|---|---|---|---|---|---|
| P1 | **Guard `uploadmedia` against stand-in media** | `tools/pack_media.py`, `tools/media_upload.py`, `tools/test_pack_media.py` | F-01 | prevents a wiped media partition | none | no |
| P2 | **Keep the cluster alive without WiFi** | `nettask.cpp` (+ `netplan.h` test): skip the 15-min WiFi-down restart while GAUGES is on and the OBD link is Live; show the reason on restart | F-02 | cluster stays up | low (black only) | yes |
| P3 | **Car checks owed table + BACKLOG refresh** | `docs/BACKLOG.md`, `CHANGELOG.md` (#92 to #94), PR template line | F-03, F-35 | one list of what to drive | none | no |
| P4 | **Docs catch-up** | README gauges bullet, ARCHITECTURE intro, Layout (generated), Ride; CONTRIBUTING layout and tools wording; SECURITY.md three points; CREDITS; stale comments in `platformio.ini` and `ci.yml` | F-04, F-23, F-24, N-12 | none | none | no |
| P5 | **Yellow heap margin** | `ledpanel.cpp`: music task gate or boot creation, `deckSearch` uses `room()`, `/say` as `HTTP_GET`; threshold only after a measured drive | F-05, F-06, F-08 | 0 B; removes ~4.4 KB surprise and a POST DoS | low | yes (a capture with pad, lights, music, karaoke) |
| P6 | **Black TLS and DMA headroom** | `client.cpp` (`s_api.stop()` before refresh, `snprintf` body), `audioport.cpp` TX-only I2S, delete `mic_es8311` | F-07, F-14, F-15, F-17 | ~20 KB at refresh, ~4.1 KB per clip | low to medium (audio path) | yes |
| P7 | **CI coverage and required checks** | `ci.yml`: diag + `_ota` envs, `restore-keys`, `timeout-minutes`, `cancel-in-progress` off on main, `pip install pillow`; ruleset required checks (João) | F-10, F-11, F-12, F-20, F-27 | catches shared-header breaks | none | no |
| P8 | **CI gates (optional)** | RAM % parse, size-delta comment, `.elf`/`.map` artefacts, cppcheck job, ruff `F`, actionlint | section 3.9 gap table; each passes today | a bisectable heap history | none | no |
| P9 | **Release workflow and v3.0.0** | `release.yml` on `v*` tags (three `.bin` + SHA-256, no media); boot screen shows the commit count; decision to tag is João's | F-13 | named builds to roll back to | none | yes only for the boot string |
| P10 | **Test gaps** | NMEA absurd/range, `numAt` ERANGE, wrap tests for 7 timers, `version`, `check_flash`, `/fill` parser parity, font hash test | F-09, F-21, F-22, F-28 | rules in CONTRIBUTING enforced | none | no |
| P11 | **Hygiene** | delete dead code (`healthview` only if confirmed), unused `[common]` embed list, empty overrides, `SMOOTH_FONT` unflag on ledpanel (~1.3 KB), s3 filter excludes `httpget.cpp`, `reqauth` borrowed params, `$PID` in `ntest.ps1`, `.gitattributes`, serial row bound | F-17, F-25, F-29, N-01 to N-10 | -500 to -700 lines, ~1.3 KB flash | low | yes for `SMOOTH_FONT` |
| P12 | **Design records and branches** | index rows and status fixes, move 10 obsolete files to `docs/superpowers/archive/`; João deletes 28 merged PR branches | F-33, F-34 | none | none | no |
| P13 | **Cockpit legibility** | `termpal.cpp` ALERT and blue DIM, link messages out of the 17 px face; phone page `flex-wrap`; document the NaN gate | F-18, F-19, F-30, F-31, F-32 | none | owner picks colours by eye | yes (panel and phone) |
| P14 | **Split `ledpanel.cpp` and black `loop()`** (largest; do last) | along the 13 section seams; pad state machine and per-screen branches into `util/` with tests | F-16 | readability | heap-neutral if statics stay static | yes (pad feel, `[mem]`) |

Order that pays most per effort: P1, P3, P4, P7, P5, P6, P2, then the rest. Anything touching `platformio.ini` or `src/util/wire/` affects both decks and goes one at a time with both decks rebuilt.

## 6. Verified vs owed (car checks)

"Verified" below means checked on a deck or in the car by João according to BACKLOG, CHANGELOG or the PR bodies. This review verified nothing on a device. Age is days since merge to 2026-11-24.

| PR | Feature | Age | Verified? | Tracked in BACKLOG? |
|---|---|---|---|---|
| #38 | camera alerts | 17 | owed (about 21 bench items) | yes |
| #47 | LED welcome intro | 16 | desk-checked; car owed; sends nothing while `LED_EFFECTS 0` | no |
| #55 | black karaoke | 15 | not driven | one sentence |
| #57 | PS3 pad redesign | 12 | owed | yes |
| #58 | IR remote (JUMP/FADE, in car) | 12 | desk-checked; effects and range owed | yes |
| #62 | terminal line and link tags | 11 | yellow desk-checked; car owed | no |
| #66 | pad + lights restarts (heapguard) | 9 | both decks flashed; 20 min drive owed | yes |
| #69 to #72 | OBD gauges, street look, Suki, Drive | 6 | car test owed | no |
| #73 to #75 | trip + fuel book, Trip ticker | 5 | car test owed | no |
| #81, #82 | Ride, follows the yellow colour | 5 | both decks flashed; car test owed | no |
| #83 | drive log (sink reverted in #84) | 4 | capture merged; no consumer yet | no |
| #87 to #91 | Cockpit and its look, load every 3 s | 3 to 4 | host-tested only | no |
| #92 to #94 | PS3 press queue, pink | 2 to 3 | #94 flashed and judged by eye by the owner; #92 and #93 not on the road | no |

13 PR groups owed a check; 5 tracked in BACKLOG (#38, #57, #58, #66, and #55 as a sentence); 8 untracked.

## 7. Not reviewed, and why

- **Everything that needs a deck (Phase 5, not run by decision 1):** runtime heap, largest block, PSRAM, stack high-water marks, allocation failures, frame times, touch and pad latency, boot times, OTA behaviour, resets per capture, screenshots, night legibility. Every such number is "not measured".
- **32-bit host run:** `NTEST_FLAGS=-m32` could not complete on this PC: the installed MinGW (WinLibs 16.2) has no 32-bit `libstdc++` (`ld: cannot find -lstdc++`), so every suite failed at link (0 of 140 linked): a toolchain limit, not test failures. CI runs the 32-bit pass on Linux; this review did not re-verify it.
- **Tools not installed, so skipped:** gitleaks (a history grep was used instead), clang-tidy, actionlint, PSScriptAnalyzer, Connect IQ build (the SDK exists on the PC but Monkey C was only read). `pio check` and `pio pkg outdated` were not run (they would touch `.pio` and the pins).
- **Not produced:** `-Wconversion` family counts, per-object map-file contributions, host line coverage (`gcovr`), the 3x flakiness run, an OBD polling replay, `cppcheck` on `ledpanel.cpp` (lacks Arduino and Bluedroid headers), regeneration of `ca_bundle.inc`, `dex_data.inc`, `status_sprite.inc` and `genre_data.inc` (live network data).
- **Sampled, not exhaustive:** `%s` null-safety (30 of 137 sites), `strncpy` (13) and `memcpy` (90) with computed sizes; 26 suites against CONTRIBUTING's rules by keyword; the 28-stale-branch claim (matched by PR title, not per-branch content); concurrency and failure tables (read from source; BLE and Bluedroid callback context was not traced into the stack libraries).
- **GitHub settings not visible to `gh`:** fork-PR approval policy, reviewer rules beyond the ruleset.
- **Assumptions in the font-size finding:** 60 cm viewing distance, 57.6 mm panel width, ISO 15008's 16 arcmin quoted from memory. The phone-page overflow was measured in headless Edge, not on a phone font.
- **Reviewer-kit self-test** (`esp32-reviewer`, `obd2-reviewer` on #90 to #92): not run.
- **Out of scope by the plan:** fixes, parked branches (`feat/voice-agent`, `claude/intro-greetings-8b8b67`), private media, other repositories.
