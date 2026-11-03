# Boot Sound Rosters Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** At each boot, the black deck plays a random clip, never last boot's, at each of three
moments: the silver Honda's drive greeting, the montage start and the Honda logo. Each comes from
a roster the owner picked.

**Architecture:**
- **Storage:** each slot's clips are packed into one git-ignored roster file in the media
  partition (`data/drive.snd`, `opener.snd`, `finale.snd`). A pure reader (`util/black/sndroster`)
  and a pure picker (`util/black/soundpick`) are host-tested.
- **Picking:** `main.cpp` picks the three at boot with NVS memory of last boot's picks. The
  montage-start pick is limited to clips that fit before the chosen logo clip.
- **Playing:** `audio/greeting` plays the picks, falling back to today's single files when a
  roster is missing.

**Tech Stack:**
- C++17 on ESP32-S3 (Arduino, PlatformIO env `s3`, -Werror on `src/`), Unity host tests through
  `tools/run_tests.ps1`.
- Python 3 standard library for the tool, with `unittest`.

**Spec:** `docs/superpowers/specs/2026-11-03-boot-sound-rosters-design.md`

## Global Constraints

- **Branch:** `claude/boot-sound-rosters` (from `main`). Don't merge, and don't open a PR unless
  João asks.
- **Commits:** `type(scope): subject`, ending with
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- **No audio, personal paths or vault paths committed.** `data/*.snd` and `data/rosters.txt` are
  git-ignored. Only `data/rosters.example.txt` is committed.
- **No new RAM buffers:** clips play from flash in place, as today. Keep fonts and buffers small,
  because the Spotify TLS heap is tight.
- **Clip format:** 16-bit signed little-endian mono at 16000 Hz (`gen_greeting.RATE`), max 4 s
  per clip.
- **Media pack:** at most 32 files (`pack_media.MAX_FILES`) and the 0x9E0000 partition.
- **McQueen's `greeting_alt.pcm`, the intro videos' sound and the howl are unchanged.**
- **Fallback with no roster:** today's `greeting.pcm` (or the embedded chime), `opener.pcm` and
  `finale.pcm`.
- **NVS:** namespace `boot`, keys `drive`, `opener`, `finale` (int, last index), next to `last`.
- **Ask João before flashing COM12** (firmware or media). After a manual download mode, press RESET.

## Review Focus

1. **A fresh clone with no `.snd` files** must build and boot with today's sounds. The firmware
   reads a missing or stand-in file as count 0. Test: `test_sndroster` `test_empty_and_garbage`.
2. **NVS holding an index from a longer, older roster** (the owner shrinks a roster). The pick must
   stay in range. Test: `test_soundpick` `test_bad_last_is_ignored`.
3. **No montage-start clip fits before the logo clip:** no opener plays, and there's no crash.
   Test: `test_soundpick` `test_none_fit_gives_minus_one`.
4. **Packing media from the worktree:** its `data/` has only stand-ins, so `uploadmedia` from
   there would wipe João's real montage and videos. Task 5 copies the real media in first.
5. **A clip name longer than the 24-byte field:** the tool truncates it to 23 characters and the
   reader never reads past the field. Tests: `test_gen_rosters.test_long_name_truncated`, and
   `test_sndroster` `test_name_needs_nul`.

---

### Task 1: Roster file reader (`util/black/sndroster`)

**Files:**
- Create: `src/util/black/sndroster.h`, `src/util/black/sndroster.cpp`
- Test: `test/test_sndroster/test_sndroster.cpp`

**Interfaces:**
- Produces:
```cpp
// A slot's boot clips (tools/gen_rosters.py): "SNDR", u16 version, u16 count, then count x
// {u32 offset from the file's start, u32 size in bytes, char name[24] NUL-padded}, then the clips
// (16-bit mono 16 kHz PCM, 2-byte aligned). Little-endian. PURE, host-tested.
namespace sndroster {
constexpr size_t HEAD = 8, ENTRY = 32, NAME = 24;
constexpr uint16_t VERSION = 1;
constexpr int MAX = 32;
struct Clip { const uint8_t* data; size_t bytes; const char* name; };
int count(const uint8_t* blob, size_t len);   // 0 when invalid (bad magic/version, count > MAX, table past len)
bool at(const uint8_t* blob, size_t len, int i, Clip* out);   // false when i is out of range or the entry is bad
}
```

- [ ] **Step 1: Write the failing tests.** Build blobs in the test with a small helper,
  `make(names, sizes)`. Tests:
  - `test_reads_clips`: two clips, "bem" (4 bytes) and "okaeri" (6 bytes).
    `count == 2`, and `at(…,1,&c)` gives `c.bytes == 6`, `strcmp(c.name,"okaeri") == 0`, and
    `c.data` equal to the bytes written.
  - `test_empty_and_garbage`: `count(nullptr,0) == 0`. A 2-byte blob `{0,0}` (the pre-build
    stand-in), "SNDX", version 2, and count 33 all give 0.
  - `test_truncated`: the table runs past `len` → 0. An entry whose offset + size runs past
    `len` → `at` is false. `at(-1)` and `at(count)` are false.
  - `test_name_needs_nul`: an entry whose 16 name bytes have no NUL → `at` is false.
- [ ] **Step 2:** Run `pwsh -NoProfile -File tools/ntest.ps1 test/test_sndroster/test_sndroster.cpp src/util/black/sndroster.cpp`. Expected: compile FAIL (no header).
- [ ] **Step 3:** Implement it in `sndroster.cpp`, reading little-endian bytes like `mediapack.cpp`.
- [ ] **Step 4:** Rerun. Expected: `4 Tests 0 Failures`.
- [ ] **Step 5:** Commit `feat(boot): read the boot sound roster files`.

### Task 2: The picker (`util/black/soundpick`)

**Files:**
- Create: `src/util/black/soundpick.h`, `src/util/black/soundpick.cpp`
- Test: `test/test_soundpick/test_soundpick.cpp`

**Interfaces:**
- Produces:
```cpp
// One clip of a slot's roster each boot: random, never last boot's (NVS, main.cpp), only from the
// clips `ok` allows (nullptr = all). PURE, host-tested.
namespace soundpick {
constexpr int MAX = 32;                                   // == sndroster::MAX
int pick(uint32_t rnd, int last, int count, const bool* ok = nullptr);   // -1 when count <= 0 or none ok
// The test hook -D SOUND_FORCE=\"drive:3,opener:0\": slot's index if present and < count, else -1.
int forced(const char* spec, const char* slot, int count);
}
```

- [ ] **Step 1: Write the failing tests:**
  - `test_never_repeats_last_and_reaches_all`: count 5, every `last` in -1..4, 64 rnds
    (`r*2654435761u`). The pick is never `last`, and all 5 are seen.
  - `test_single_clip_repeats`: count 1, last 0 → 0.
  - `test_bad_last_is_ignored`: last 99 or -7 with count 3 → in [0,3). count 0 → -1. Counts
    above MAX are clamped to MAX.
  - `test_ok_filter`: `ok = {false,true,false,true}`, last -1 → only 1 or 3. With last 1 → 3.
  - `test_none_fit_gives_minus_one`: all `ok` false → -1.
  - `test_forced`: `forced("drive:3,opener:0","opener",5) == 0`,
    `forced("drive:3,opener:0","drive",5) == 3`, `forced("drive:3","finale",5) == -1`,
    `forced("drive:9","drive",5) == -1`, `forced(nullptr,"drive",5) == -1`.
- [ ] **Step 2:** Run ntest on it. Expected: compile FAIL.
- [ ] **Step 3:** Implement it as `intropick::pick` does (build the allowed list, drop `last` when
  more than one remains, then `rnd % n`). `forced` scans `slot:` followed by digits.
- [ ] **Step 4:** Rerun. Expected: `6 Tests 0 Failures`.
- [ ] **Step 5:** Commit `feat(boot): pick a boot sound per slot, never last boot's`.

### Task 3: `tools/gen_rosters.py` and the media plumbing

**Files:**
- Create: `tools/gen_rosters.py`, `tools/test_gen_rosters.py`, `data/rosters.example.txt`
- Modify:
  - `tools/pack_media.py` (`FILES` += `"drive.snd", "opener.snd", "finale.snd"`).
  - `tools/media_sizes.py` (`FILES` += the same three). Zero-filled stand-ins read as no roster,
    which is fine.
  - `tools/greeting_clip.py`: write a 2-byte stand-in for each missing `.snd`, so a fresh clone
    packs nothing for it.
  - `.gitignore` (`data/*.snd`, `data/rosters.txt`).

**Interfaces:**
- Consumes: `gen_greeting.read_wav`, `resample`, `trim_normalize`, `RATE`; the Task 1 format.
- Produces: the CLI `python tools/gen_rosters.py [rosters.txt] [--sounds DIR]`. The defaults are
  `data/rosters.txt` and `greeting-sounds/` at the repo root. It writes `data/drive.snd`,
  `data/opener.snd` and `data/finale.snd`, and prints `name  1.23 s` per clip. Module functions
  `parse(text) -> {"drive": [paths], "opener": [...], "finale": [...]}` and
  `build(clips: [(name, samples_float_list)]) -> bytes`.
- **`rosters.txt` format:** `[drive]` / `[opener]` / `[finale]` headers, one WAV path per line,
  `#` comments, blank lines ignored. The clip name is the file's base name without
  `_16k.wav`/`.wav`, truncated to 23 characters.

- [ ] **Step 1: Write the failing tests** in `test_gen_rosters.py` (as `test_pack_media.py` does):
  - `test_parse`: sections, comments, blank lines, an unknown section → `SystemExit`.
  - `test_build_layout`: two clips → `"SNDR"`, version 1, count 2, offsets 2-aligned and inside,
    sizes = 2 × samples, NUL-padded names.
  - `test_long_name_truncated`: a 20-character name → 15 characters plus NUL.
  - `test_too_long_clip`: 4.01 s at 16 kHz → `SystemExit`.
  - `test_missing_wav`: `SystemExit` naming the path.
- [ ] **Step 2:** `python tools/test_gen_rosters.py`. Expected: ImportError/FAIL.
- [ ] **Step 3:** Implement it. `data/rosters.example.txt` lists the three sections with two
  commented example lines each, and no personal paths.
- [ ] **Step 4:** `python tools/test_gen_rosters.py` and `python tools/test_pack_media.py`.
  Expected: OK.
- [ ] **Step 5:** Commit `feat(tools): build the boot sound rosters from a picks list`.

### Task 4: Play the picks (`audio/greeting`, `main.cpp`, `ui/bootscene`)

**Files:**
- Modify: `src/black/audio/greeting.h/.cpp`, `src/black/main.cpp` (`playOpening`, next to
  `pickOpening`), `src/black/ui/bootscene.h/.cpp`.

**Interfaces:**
- Consumes: `sndroster::count/at`, `soundpick::pick/forced`, `bootanim::finaleStartFrame/openerFits`,
  `montage::parse/count/fps`.
- Produces:
```cpp
namespace greeting {
enum Slot : uint8_t { DRIVE = 0, OPENER = 1, FINALE = 2 };
int rosterCount(Slot s);                 // clips in data/<slot>.snd; 0 = none (today's single file)
uint32_t rosterMs(Slot s, int i);
const char* rosterName(Slot s, int i);
void pick(Slot s, int i);                // this boot's clip; -1 or never called = today's file
}
namespace bootscene {
// Whether an opener of openerMs ends before a finale of finaleMs starts on this montage (the rule
// the scene applies; parses data/montage.bin's header). True when there's no finale.
bool openerFits(uint32_t openerMs, uint32_t finaleMs);
}
```
- **Behaviour:**
  - `greetingClip()` for `MAIN` returns the picked drive clip when one was picked. `play()`,
    `durationMs()` and `level()` follow it.
  - `playOpener()`, `openerMs()`, `playFinale()` and `finaleMs()` return the picked clip when
    picked, else today's file.
  - The picked clips are `Clip` values in static storage, named by the roster name, because the
    play task keeps the pointer.
  - In `main.cpp`, a `pickSounds(opening)` runs after `pickOpening`:
    1. finale: `soundpick::pick(esp_random(), nvs "finale", rosterCount(FINALE))`.
    2. opener: `ok[i] = bootscene::openerFits(rosterMs(OPENER,i), picked finale ms or
       greeting::finaleMs())`, then pick.
    3. drive: picked only when `opening == intropick::HONDA`; the NVS key is written only then.
  - `#ifdef SOUND_FORCE`: `soundpick::forced` overrides each slot it names.
  - It logs `[boot] sounds: drive=<name|file> opener=<name|none|file> finale=<name|file>`.
  - `bootscene.cpp` keeps its own `openerFits` check before `playOpener()`, as a guard.

- [ ] **Step 1:** Implement the greeting API, `bootscene::openerFits` (sharing
  `MONTAGE_SOUND_AT_START` and the `finaleAt` rule with the scene's montage branch, not a copy of
  it) and `pickSounds` in `main.cpp`.
- [ ] **Step 2:** `pio run -e s3` in the background. Expected: `[SUCCESS]` with -Werror.
- [ ] **Step 3:** `tools/run_tests.ps1` in the background. Expected: `fail=0`, including
  `test_sndroster` and `test_soundpick`.
- [ ] **Step 4:** Commit `feat(boot): random drive, montage and logo sounds from the rosters`.

### Task 5: The owner's rosters, docs, credits and the desk check

**Files:**
- Create (git-ignored): `data/rosters.txt`, with the spec's three lists. `now_*` lives under
  `listen/`.
- Modify: `docs/ARCHITECTURE.md` (the Greeting bullet, the media list in the S3 media section,
  `soundpick` and `sndroster` in the util list, and `SOUND_FORCE` in Test hooks), `CHANGELOG.md`
  [Unreleased], `CREDITS.md`, `data/media_sizes.txt` (`python tools/media_sizes.py record`, after
  generating).

- [ ] **Step 1: Put the real media in this worktree before packing.** Copy João's real `data/`
  media from the main checkout `D:/dev/projects/spotify-pokemon-deck/data/` into this worktree's
  `data/`: every file in `pack_media.FILES` that exists there. Then generate:
  `python tools/gen_rosters.py --sounds D:/dev/projects/spotify-pokemon-deck/greeting-sounds`.
  Expected: 10 + 14 + 12 clips listed, none over 4 s. Copy the three `.snd` files back to the
  main checkout's `data/` too, so a later build from there has them.
- [ ] **Step 2:** `python tools/pack_media.py`. Expected: 19 files, under 100 % of the partition.
- [ ] **Step 3:** Docs.
  - CREDITS.md: a row for the amitaro clips (credit "声: あみたろの声素材工房", amitaro.net), Pocket
    Sound and 効果音ラボ (free, credit not required), and myinstants clips (personal use, not
    redistributed).
  - CHANGELOG: an **Added** line for the random sounds.
  - Run `python tools/media_sizes.py record`.
- [ ] **Step 4:** Commit `docs(boot): sound rosters in the architecture, credits and changelog`
  and push.
- [ ] **Step 5: Desk check.** Ask João first. Then:
  - `pio run -e s3 -t upload --upload-port COM12`, with `-D INTRO_FORCE=\"honda\"` via
    `PLATFORMIO_BUILD_FLAGS` and cleared afterwards.
  - `pio run -e s3 -t uploadmedia --upload-port COM12`.
  - Capture 3 boots with `python tools/capture_serial.py COM12 40 <log> --no-reset`, power-cycling
    in between. Expected: each `[boot] sounds:` line differs from the one before it, slot by slot;
    `[greeting] opener` always finishes before `[greeting] finale` starts; there's no
    `no memory for its task`.
  - Reflash without the flag at the end.
