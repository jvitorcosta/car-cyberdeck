# Random Intro Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The new deck (S3) boots in 15–25 s with one random opening (Sabrina kiss video, pixel Honda + "bem vindo moreno", or pixel McQueen + "eu sou a velocidade"), then a shorter photo montage and a "POWERED BY HONDA" end card. The boot log runs over every opening.

**Architecture:** A pure picker (`util/intropick`) chooses the opening (never last boot's, NVS) and its greeting. The night drive reads its car through one interface (`ui/carlook`: the committed Honda or the git-ignored McQueen, `util/carbin`). The on-board terminal moves out of `bootscene.cpp` into `ui/bootterm`, so `ui/intro` can composite it into the kiss video's JPEG blocks (`util/termblend`, pure). `bootscene` gains a "no drive" start that goes straight to the montage.

**Tech Stack:** C++17 / Arduino-ESP32 / TFT_eSPI / JPEGDEC / tjpgd / FreeRTOS; Unity host tests via `tools/ntest.ps1`; Python 3 tools (stdlib + ffmpeg); PixelLab MCP tools for the McQueen art.

**Spec:** `docs/superpowers/specs/2026-10-19-random-intro-design.md`

## Global Constraints

- Only the `s3` build changes behaviour. `cyd` and `cyd_companion` must still build, and the old deck keeps today's boot (its flash is 96.6 % full: nothing new may be embedded there).
- Power-on to the music screen: 15 to 25 s depending on the opening (kiss ~19 s, Honda ~16 s, McQueen ~15 s).
- Owner assets stay git-ignored: `data/intro.*`, `data/montage.bin`, `data/mcqueen.bin`, `montage-photos/`, the McQueen PNG. Each embedded optional file gets a pre-build script that writes an empty/one-sample file when absent.
- Commits: conventional (`type(scope): subject`), ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Tests: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_tests.ps1` (all suites) or `tools/ntest.ps1 <test.cpp> <modules.cpp...>` (one); Python: `python -m unittest discover -s tools -p "test_gen_*.py"`.
- Builds: `pio run -e s3`, `pio run -e cyd`, `pio run -e cyd_companion`. Flash: `pio run -e s3 -t upload --upload-port COM12`.
- File writes from Python heredocs: use raw strings (`r'''...'''`) for any C++ containing `\n`; earlier sessions turned `\\n` into real newlines.
- No SD card code (the parked `SD_CACHE` stays off).

## Review Focus

1. **A build without `intro.bin` and without `mcqueen.bin`** (a fresh clone, CI): every boot must pick the Honda, whatever NVS says. Pinned in Task 1 (`test_only_honda_without_files`).
2. **A corrupt or out-of-range `last` in NVS** (first boot reads the default, a future build with more openings wrote 5): the pick must still be a valid, available opening. Pinned in Task 1 (`test_bad_last_is_ignored`).
3. **`INTRO_FORCE` naming an opening the build lacks** (forcing `kiss` without a video): the deck must fall back to a normal pick, not hang on an empty video. Pinned in Task 1 (`test_forced_needs_its_files`).
4. **A truncated or wrong-size `mcqueen.bin`** (an interrupted tool run): the parser must refuse it, so McQueen isn't offered and nothing reads past the blob. Pinned in Task 3 (`test_rejects_truncated_and_bad_magic`).
5. **JPEG blocks partly over, or wholly outside, the terminal panel** (JPEGDEC hands out 16-px MCU rows of varying width; the panel is 206 × 60 at 4,4): only the overlapping pixels may change. Pinned in Task 7 (`test_block_partly_over_the_panel`, `test_block_outside_is_untouched`).

---

### Task 1: The opening picker (`util/intropick`)

**Files:**
- Create: `src/util/intropick.h`, `src/util/intropick.cpp`
- Test: `test/test_intropick/test_intropick.cpp`

**Interfaces:**
- Produces:
  - `enum intropick::Opening : uint8_t { KISS = 0, HONDA = 1, MCQUEEN = 2, COUNT = 3 };`
  - `enum intropick::Greeting : uint8_t { NONE = 0, MAIN = 1, ALT = 2 };`
  - `intropick::Opening intropick::pick(uint32_t rnd, int last, bool haveKiss, bool haveMcQueen);`
  - `intropick::Greeting intropick::greetingFor(intropick::Opening o);`
  - `const char* intropick::name(intropick::Opening o);` → `"kiss"`, `"honda"`, `"mcqueen"`
  - `int intropick::parse(const char* s);` → the Opening, or -1
  - `int intropick::forced(int f, bool haveKiss, bool haveMcQueen);` → f when that opening is available, else -1

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_intropick/test_intropick.cpp
#include <unity.h>
#include "../../src/util/intropick.h"

using namespace intropick;

void setUp() {}
void tearDown() {}

void test_never_repeats_last_and_reaches_all() {
    bool seen[COUNT] = {};
    for (int last = 0; last < COUNT; ++last)
        for (uint32_t r = 0; r < 64; ++r) {
            const Opening o = pick(r * 2654435761u, last, true, true);
            TEST_ASSERT_NOT_EQUAL(last, (int)o);
            seen[o] = true;
        }
    for (bool s : seen) TEST_ASSERT_TRUE(s);
}

void test_skips_missing_files() {
    for (uint32_t r = 0; r < 64; ++r) {
        TEST_ASSERT_NOT_EQUAL(KISS, pick(r, MCQUEEN, false, true));
        TEST_ASSERT_NOT_EQUAL(MCQUEEN, pick(r, KISS, true, false));
    }
}

// Review focus 1: a fresh clone has neither file.
void test_only_honda_without_files() {
    for (int last = -1; last < COUNT; ++last)
        for (uint32_t r = 0; r < 16; ++r) TEST_ASSERT_EQUAL(HONDA, pick(r, last, false, false));
}

// Review focus 2: NVS may hold anything.
void test_bad_last_is_ignored() {
    for (uint32_t r = 0; r < 32; ++r) {
        const Opening o = pick(r, 5, true, true);
        TEST_ASSERT_TRUE(o < COUNT);
        TEST_ASSERT_TRUE((int)pick(r, -7, true, false) == KISS || (int)pick(r, -7, true, false) == HONDA);
    }
}

void test_greeting_per_opening() {
    TEST_ASSERT_EQUAL(NONE, greetingFor(KISS));
    TEST_ASSERT_EQUAL(MAIN, greetingFor(HONDA));
    TEST_ASSERT_EQUAL(ALT, greetingFor(MCQUEEN));
}

void test_names_round_trip() {
    for (int o = 0; o < COUNT; ++o) TEST_ASSERT_EQUAL_INT(o, parse(name((Opening)o)));
    TEST_ASSERT_EQUAL_INT(-1, parse("sonic"));
    TEST_ASSERT_EQUAL_INT(-1, parse(""));
    TEST_ASSERT_EQUAL_INT(-1, parse(nullptr));
}

// Review focus 3: forcing an opening the build lacks falls back (-1).
void test_forced_needs_its_files() {
    TEST_ASSERT_EQUAL_INT(KISS, forced(KISS, true, false));
    TEST_ASSERT_EQUAL_INT(-1, forced(KISS, false, true));
    TEST_ASSERT_EQUAL_INT(-1, forced(MCQUEEN, true, false));
    TEST_ASSERT_EQUAL_INT(HONDA, forced(HONDA, false, false));
    TEST_ASSERT_EQUAL_INT(-1, forced(-1, true, true));
    TEST_ASSERT_EQUAL_INT(-1, forced(9, true, true));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_never_repeats_last_and_reaches_all);
    RUN_TEST(test_skips_missing_files);
    RUN_TEST(test_only_honda_without_files);
    RUN_TEST(test_bad_last_is_ignored);
    RUN_TEST(test_greeting_per_opening);
    RUN_TEST(test_names_round_trip);
    RUN_TEST(test_forced_needs_its_files);
    return UNITY_END();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/ntest.ps1 test/test_intropick/test_intropick.cpp`
Expected: COMPILE FAILED (`intropick.h` missing).

- [ ] **Step 3: Write the implementation**

```cpp
// src/util/intropick.h
#pragma once
#include <cstdint>
// The S3's boot opening, picked at random each boot (spec 2026-10-19-random-intro): the kiss
// video, the pixel Honda's night drive or the pixel McQueen's. Never last boot's (kept in NVS by
// main.cpp) and never one whose files the build lacks; the Honda is always there. PURE, host-tested.
namespace intropick {
enum Opening : uint8_t { KISS = 0, HONDA = 1, MCQUEEN = 2, COUNT = 3 };
enum Greeting : uint8_t { NONE = 0, MAIN = 1, ALT = 2 };   // data/greeting.pcm, greeting_alt.pcm

// last: the previous boot's opening (anything out of range = none).
Opening pick(uint32_t rnd, int last, bool haveKiss, bool haveMcQueen);
Greeting greetingFor(Opening o);   // kiss: none (the video has its sound)
const char* name(Opening o);       // "kiss", "honda", "mcqueen" (logs, INTRO_FORCE)
int parse(const char* s);          // name() back to an Opening; -1 otherwise
int forced(int f, bool haveKiss, bool haveMcQueen);   // f if it's an available opening, else -1
}
```

```cpp
// src/util/intropick.cpp
#include "intropick.h"
#include <cstring>

namespace intropick {

static bool available(int o, bool haveKiss, bool haveMcQueen) {
    return o == HONDA || (o == KISS && haveKiss) || (o == MCQUEEN && haveMcQueen);
}

Opening pick(uint32_t rnd, int last, bool haveKiss, bool haveMcQueen) {
    Opening list[COUNT];
    int n = 0;
    for (int o = 0; o < COUNT; ++o)
        if (available(o, haveKiss, haveMcQueen)) list[n++] = (Opening)o;
    if (n > 1) {                       // drop last boot's, when there's another
        int k = 0;
        for (int i = 0; i < n; ++i)
            if (list[i] != last) list[k++] = list[i];
        n = k;
    }
    return list[rnd % (uint32_t)n];
}

Greeting greetingFor(Opening o) { return o == HONDA ? MAIN : o == MCQUEEN ? ALT : NONE; }

const char* name(Opening o) {
    static const char* const NAMES[COUNT] = {"kiss", "honda", "mcqueen"};
    return o < COUNT ? NAMES[o] : "?";
}

int parse(const char* s) {
    if (!s) return -1;
    for (int o = 0; o < COUNT; ++o)
        if (strcmp(s, name((Opening)o)) == 0) return o;
    return -1;
}

int forced(int f, bool haveKiss, bool haveMcQueen) {
    return f >= 0 && f < COUNT && available(f, haveKiss, haveMcQueen) ? f : -1;
}

}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/ntest.ps1 test/test_intropick/test_intropick.cpp src/util/intropick.cpp`
Expected: `7 Tests 0 Failures 0 Ignored`

- [ ] **Step 5: Commit**

```bash
git add src/util/intropick.h src/util/intropick.cpp test/test_intropick/test_intropick.cpp
git commit -m "feat(s3): pick the boot opening at random, never last boot's"
```

---

### Task 2: The greeting follows the opening

The coin flip (`util/greetpick`) goes; `main.cpp` tells `greeting` which clip this boot plays.

**Files:**
- Modify: `src/audio/greeting.h`, `src/audio/greeting.cpp:69-80` (`pickGreeting`, `greetingClip`, `play`)
- Delete: `src/util/greetpick.h`, `test/test_greetpick/`

**Interfaces:**
- Consumes: `intropick::Greeting` (Task 1).
- Produces: `void greeting::choose(intropick::Greeting g);` — call before `play()`; the default is `MAIN` (the CYD never calls it). With `NONE`, `play()` does nothing, `durationMs()` is 0 and `level()` is 0.

- [ ] **Step 1: Replace the pick in `greeting.cpp`**

Remove `#include "../util/greetpick.h"`, `pickGreeting()` and the cached `greetingClip()`. Add:

```cpp
#include "../util/intropick.h"

// This boot's greeting (main.cpp chooses it from the opening before play()); MAIN by default.
static intropick::Greeting s_choice = intropick::MAIN;
static const Clip SILENT = {nullptr, 0, "greeting", &audiosink::GREETING_VOLUME, CLIP_RATE};

void choose(intropick::Greeting g) { s_choice = g; }

// The clip play(), durationMs() and level() agree on (the boot scene's timing and car lights).
static const Clip& greetingClip() {
    if (s_choice == intropick::NONE) return SILENT;
#ifdef BOARD_S3
    if (s_choice == intropick::ALT && present(GREETING_ALT)) return GREETING_ALT;
#endif
    return GREETING;
}
```

Change `play()`:

```cpp
void play() {
    if (s_choice == intropick::NONE) return;   // the kiss video has its own sound
    start(&greetingClip(), "greeting");
}
```

Check `level()`: `loudness::loudness(nullptr, 0, ...)` must return 0. Read `src/util/loudness.cpp`; if it doesn't guard `samples == 0`, add at the top of `level()`:

```cpp
    if (c.samples == 0) return 0;
```

- [ ] **Step 2: Declare it in `greeting.h`**

After `void play();` add:

```cpp
// This boot's greeting, from the S3's opening (util/intropick): MAIN (data/greeting.pcm), ALT
// (data/greeting_alt.pcm) or NONE (play() does nothing, durationMs() is 0). MAIN unless called.
void choose(intropick::Greeting g);
```

with `#include "../util/intropick.h"` at the top. Update `play()`'s comment: it plays the chosen greeting.

- [ ] **Step 3: Delete the coin flip and its test**

```bash
git rm -r src/util/greetpick.h test/test_greetpick
```

- [ ] **Step 4: Build both decks and run the suite**

Run: `pio run -e s3` and `pio run -e cyd` → both `SUCCESS`. Then `tools/run_tests.ps1` → `fail=0` (one suite fewer, one more from Task 1).

- [ ] **Step 5: Commit**

```bash
git add -A src/audio src/util test
git commit -m "refactor(s3): the greeting follows the boot opening, not a coin flip"
```

---

### Task 3: One car interface (`util/carbin`, `ui/carlook`)

The night drive draws "a car" instead of `car_sprite::`, so McQueen can drive the same scene. No visible change in this task.

**Files:**
- Create: `src/util/carbin.h`, `src/util/carbin.cpp` (pure: the `CAR1` file), `src/ui/carlook.h`, `src/ui/carlook.cpp`, `tools/mcqueen_clip.py`
- Modify: `src/ui/bootscene.cpp` (`drawCar`, the wheel and light positions), `platformio.ini`, `.gitignore`
- Test: `test/test_carbin/test_carbin.cpp`

**Interfaces:**
- Produces:
  - `struct carbin::Car { int w, h, rearX, frontX, wheelY, headX, headY, tailY; const uint8_t* kinds; const uint8_t* px; };`
  - `bool carbin::parse(const uint8_t* blob, size_t len, carbin::Car* out);`
  - `bool carbin::pixel(const carbin::Car& c, int x, int y, uint16_t* rgb565, uint8_t* kind);` (kinds are `car_sprite::Kind` values)
  - `struct carlook::Look { int w, h, rearX, frontX, wheelY, headX, headY, tailY; bool (*pixel)(int x, int y, uint16_t* rgb565, uint8_t* kind); };`
  - `carlook::Look carlook::honda();`
  - `bool carlook::mcqueen(carlook::Look* out);` — false without a valid `data/mcqueen.bin` (always false on the CYD)
- File format `CAR1` (little-endian): `"CAR1"`, then u16 `w, h, rearX, frontX, wheelY, headX, headY, tailY` (20-byte header), then `ceil(w*h/2)` kind bytes (even pixel in the high nibble, as `car_sprite.inc`), then `w*h` u16 RGB565 pixels.

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_carbin/test_carbin.cpp
#include <unity.h>
#include <cstring>
#include <vector>
#include "../../src/util/carbin.h"

void setUp() {}
void tearDown() {}

// A 3x2 car: pixel (1,0) is HEAD (4) red 0xF800, (2,1) is LOWER (2) 0x07E0, the rest clear.
static std::vector<uint8_t> blob() {
    std::vector<uint8_t> b = {'C', 'A', 'R', '1'};
    const uint16_t head[8] = {3, 2, 0, 2, 1, 3, 0, 1};   // w h rearX frontX wheelY headX headY tailY
    for (uint16_t v : head) { b.push_back(v & 255); b.push_back(v >> 8); }
    const uint8_t kinds[3] = {0x04, 0x00, 0x02};          // pixels 0,1 | 2,3 | 4,5
    b.insert(b.end(), kinds, kinds + 3);
    const uint16_t px[6] = {0, 0xF800, 0, 0, 0, 0x07E0};
    for (uint16_t v : px) { b.push_back(v & 255); b.push_back(v >> 8); }
    return b;
}

void test_parses_header_and_pixels() {
    const std::vector<uint8_t> b = blob();
    carbin::Car c;
    TEST_ASSERT_TRUE(carbin::parse(b.data(), b.size(), &c));
    TEST_ASSERT_EQUAL_INT(3, c.w);
    TEST_ASSERT_EQUAL_INT(2, c.h);
    TEST_ASSERT_EQUAL_INT(3, c.headX);
    TEST_ASSERT_EQUAL_INT(1, c.tailY);
    uint16_t col = 0;
    uint8_t kind = 0;
    TEST_ASSERT_TRUE(carbin::pixel(c, 1, 0, &col, &kind));
    TEST_ASSERT_EQUAL_HEX16(0xF800, col);
    TEST_ASSERT_EQUAL_UINT8(4, kind);
    TEST_ASSERT_TRUE(carbin::pixel(c, 2, 1, &col, &kind));
    TEST_ASSERT_EQUAL_HEX16(0x07E0, col);
    TEST_ASSERT_FALSE(carbin::pixel(c, 0, 0, &col, &kind));
    TEST_ASSERT_EQUAL_UINT8(0, kind);
    TEST_ASSERT_FALSE(carbin::pixel(c, 3, 0, &col, &kind));   // outside
    TEST_ASSERT_FALSE(carbin::pixel(c, -1, 0, nullptr, nullptr));
}

// Review focus 4: an interrupted tool run or a stale format is refused.
void test_rejects_truncated_and_bad_magic() {
    std::vector<uint8_t> b = blob();
    carbin::Car c;
    TEST_ASSERT_FALSE(carbin::parse(b.data(), b.size() - 1, &c));
    TEST_ASSERT_FALSE(carbin::parse(b.data(), 10, &c));
    TEST_ASSERT_FALSE(carbin::parse(nullptr, 0, &c));
    TEST_ASSERT_FALSE(carbin::parse(b.data(), 2, &c));   // the build's one-sample stand-in
    b[3] = '2';
    TEST_ASSERT_FALSE(carbin::parse(b.data(), b.size(), &c));
}

void test_rejects_zero_or_huge_size() {
    std::vector<uint8_t> b = blob();
    b[4] = 0; b[5] = 0;                                   // w = 0
    carbin::Car c;
    TEST_ASSERT_FALSE(carbin::parse(b.data(), b.size(), &c));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_parses_header_and_pixels);
    RUN_TEST(test_rejects_truncated_and_bad_magic);
    RUN_TEST(test_rejects_zero_or_huge_size);
    return UNITY_END();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/ntest.ps1 test/test_carbin/test_carbin.cpp`
Expected: COMPILE FAILED (`carbin.h` missing).

- [ ] **Step 3: Write `carbin`**

```cpp
// src/util/carbin.h
#pragma once
#include <cstddef>
#include <cstdint>
// A boot-scene car from a file (data/mcqueen.bin, tools/gen_mcqueen_sprite.py), read in place
// from flash. Format "CAR1" (little-endian): u16 w, h, rearX, frontX, wheelY (wheel centres),
// headX, headY (headlight beam origin), tailY (taillight glow top); then ceil(w*h/2) kind bytes
// (car_sprite::Kind, even pixel in the high nibble); then w*h u16 RGB565. PURE, host-tested.
namespace carbin {
struct Car { int w, h, rearX, frontX, wheelY, headX, headY, tailY; const uint8_t* kinds; const uint8_t* px; };
bool parse(const uint8_t* blob, size_t len, Car* out);   // false: not a whole, valid file
// As car_sprite::pixel: false when transparent or outside (*kind = 0 then).
bool pixel(const Car& c, int x, int y, uint16_t* rgb565, uint8_t* kind);
}
```

```cpp
// src/util/carbin.cpp
#include "carbin.h"
#include <cstring>

namespace carbin {

static constexpr size_t HEAD = 20;

static int u16(const uint8_t* p) { return p[0] | p[1] << 8; }

bool parse(const uint8_t* blob, size_t len, Car* out) {
    if (!blob || len < HEAD || memcmp(blob, "CAR1", 4) != 0) return false;
    Car c;
    c.w = u16(blob + 4); c.h = u16(blob + 6);
    c.rearX = u16(blob + 8); c.frontX = u16(blob + 10); c.wheelY = u16(blob + 12);
    c.headX = u16(blob + 14); c.headY = u16(blob + 16); c.tailY = u16(blob + 18);
    if (c.w <= 0 || c.h <= 0 || c.w > 320 || c.h > 240) return false;
    const size_t n = (size_t)c.w * c.h, kinds = (n + 1) / 2;
    if (len != HEAD + kinds + 2 * n) return false;
    c.kinds = blob + HEAD;
    c.px = blob + HEAD + kinds;
    *out = c;
    return true;
}

bool pixel(const Car& c, int x, int y, uint16_t* rgb565, uint8_t* kind) {
    uint8_t k = 0;
    if (x >= 0 && y >= 0 && x < c.w && y < c.h) {
        const int i = y * c.w + x;
        k = (i & 1) ? (c.kinds[i >> 1] & 0x0F) : (c.kinds[i >> 1] >> 4);
        if (k && rgb565) *rgb565 = (uint16_t)(c.px[2 * i] | c.px[2 * i + 1] << 8);
    }
    if (kind) *kind = k;
    return k != 0;
}

}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/ntest.ps1 test/test_carbin/test_carbin.cpp src/util/carbin.cpp`
Expected: `3 Tests 0 Failures 0 Ignored`

- [ ] **Step 5: Write `carlook`**

```cpp
// src/ui/carlook.h
#pragma once
#include <cstdint>
// The night drive's car: the committed Honda (ui/car_sprite) or, on the S3, the git-ignored
// McQueen (data/mcqueen.bin, util/carbin). Size, wheel centres and light anchors come with it.
namespace carlook {
struct Look {
    int w, h, rearX, frontX, wheelY;   // wheel centres (wheels are drawn in code)
    int headX, headY, tailY;           // headlight beam origin; taillight glow top (sprite coords)
    bool (*pixel)(int x, int y, uint16_t* rgb565, uint8_t* kind);
};
Look honda();
bool mcqueen(Look* out);   // false without a valid data/mcqueen.bin (always on the CYD)
}
```

```cpp
// src/ui/carlook.cpp
#include "carlook.h"
#include "car_sprite.h"
#include "../util/carbin.h"
#ifdef BOARD_S3
#include "../core/embed.h"
EMBED(data_mcqueen_bin);
#endif

namespace carlook {

// The Honda's beam starts at its nose (x 200, y 31) and its taillight glow at y 21, as the scene
// always drew them.
Look honda() {
    return {car_sprite::width(), car_sprite::height(), car_sprite::REAR_WHEEL_X,
            car_sprite::FRONT_WHEEL_X, car_sprite::WHEEL_Y, 200, 31, 21, car_sprite::pixel};
}

#ifdef BOARD_S3
static carbin::Car s_mc;
static bool mcPixel(int x, int y, uint16_t* c, uint8_t* k) { return carbin::pixel(s_mc, x, y, c, k); }
#endif

bool mcqueen(Look* out) {
#ifdef BOARD_S3
    if (!carbin::parse(EMBED_DATA(data_mcqueen_bin), EMBED_SIZE(data_mcqueen_bin), &s_mc)) return false;
    *out = {s_mc.w, s_mc.h, s_mc.rearX, s_mc.frontX, s_mc.wheelY, s_mc.headX, s_mc.headY, s_mc.tailY, mcPixel};
    return true;
#else
    (void)out;
    return false;
#endif
}

}
```

`data/mcqueen.bin` must exist for the S3 build to link, so this task also adds its pre-build script and embed entry:

`tools/mcqueen_clip.py`:

```python
# PlatformIO pre-build script: make sure data/mcqueen.bin (the S3's pixel McQueen) exists.
#
# data/mcqueen.bin is yours (tools/gen_mcqueen_sprite.py mcqueen.png) and git-ignored. Without one,
# a one-sample stand-in is written (the build can't embed an empty file); the deck then never
# picks the McQueen opening (util/carbin refuses it).
Import("env")  # noqa: F821 (provided by PlatformIO/SCons)
import os

out = os.path.join(env.subst("$PROJECT_DIR"), "data", "mcqueen.bin")  # noqa: F821
if not os.path.isfile(out):
    with open(out, "wb") as f:
        f.write(bytes(2))
    print("mcqueen_clip: no data/mcqueen.bin, the McQueen opening is off")
```

In `platformio.ini`, `[common] extra_scripts`, after the `tune_clip.py` lines:

```ini
    ; S3 pixel McQueen opening: data/mcqueen.bin from tools/gen_mcqueen_sprite.py (git-ignored), else off
    pre:tools/mcqueen_clip.py
```

In `[env:s3] board_build.embed_files`, after `data/tunes.txt`: `    data/mcqueen.bin`. Append to `.gitignore`:

```
# Pixel McQueen (a Disney/Pixar character): the art and the packed sprite stay local
data/mcqueen.bin
mcqueen*.png
```

- [ ] **Step 6: The night drive reads the Look**

In `src/ui/bootscene.cpp`:
- Add `#include "carlook.h"` and a file-scope `carlook::Look s_car = carlook::honda();`.
- In `drawCar`, replace `car_sprite::width()` → `s_car.w`, `car_sprite::height()` → `s_car.h`, `car_sprite::pixel(` → `s_car.pixel(`, `car_sprite::WHEEL_Y` → `s_car.wheelY`, `car_sprite::REAR_WHEEL_X` → `s_car.rearX`, `car_sprite::FRONT_WHEEL_X` → `s_car.frontX`. Keep `car_sprite::HEAD/TAIL/UPPER/LOWER` (the Kind values).
- The beam: `c.x + 200 + i` → `c.x + s_car.headX + i`; `c.top + 31 - h / 3` → `c.top + s_car.headY - h / 3`.
- The tail glow: `blendRect(c.x - 4, c.top + 21, 6, 9, ...)` → `blendRect(c.x - 4, c.top + s_car.tailY, 6, 9, ...)`.
- `CAR_TOP = GROUND_Y - 65` stays (both cars are 66 px tall).

- [ ] **Step 7: Build and check nothing moved**

Run: `pio run -e s3` and `pio run -e cyd` → `SUCCESS`; `tools/run_tests.ps1` → `fail=0`. Flash the S3 and watch one boot: the Honda drive looks as before.

- [ ] **Step 8: Commit**

```bash
git add src/util/carbin.* src/ui/carlook.* src/ui/bootscene.cpp test/test_carbin tools/mcqueen_clip.py platformio.ini .gitignore
git commit -m "refactor(boot): the night drive draws its car through one interface"
```

---

### Task 4: The McQueen sprite packer (`tools/gen_mcqueen_sprite.py`)

**Files:**
- Create: `tools/gen_mcqueen_sprite.py`, `tools/test_gen_mcqueen_sprite.py`
(The pre-build script, embed entry and git-ignore lines for `data/mcqueen.bin` are in Task 3.)

**Interfaces:**
- Consumes: the `CAR1` format (Task 3); `decode_png` from `tools/gen_status_sprite.py`.
- Produces: `data/mcqueen.bin`. Python API: `classify(rgba, x, y, head, tail, shoulder) -> int`, `pack(w, h, kinds, px, anchors) -> bytes`, `rgb565(r, g, b) -> int`.
- Classification (car_sprite::Kind): alpha < 128 → 0 NONE; inside the `--head` box → 4 HEAD; inside the `--tail` box → 5 TAIL; a red body pixel (r ≥ 140, g ≤ 90, b ≤ 90) → 1 UPPER above `--shoulder`, else 2 LOWER; anything else opaque → 3 OTHER.

- [ ] **Step 1: Write the failing test**

```python
# tools/test_gen_mcqueen_sprite.py
"""Host tests for tools/gen_mcqueen_sprite.py. Run: python tools/test_gen_mcqueen_sprite.py"""
import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_mcqueen_sprite as gm  # noqa: E402


class Classify(unittest.TestCase):
    HEAD, TAIL = (190, 25, 199, 34), (0, 18, 4, 28)

    def k(self, rgba, x, y):
        return gm.classify(rgba, x, y, self.HEAD, self.TAIL, 30)

    def test_transparent(self):
        self.assertEqual(self.k((200, 0, 0, 0), 50, 10), 0)

    def test_lamps_win(self):
        self.assertEqual(self.k((250, 240, 120, 255), 195, 30), 4)
        self.assertEqual(self.k((200, 10, 10, 255), 2, 20), 5)

    def test_red_paint_above_and_below_the_shoulder(self):
        self.assertEqual(self.k((210, 20, 25, 255), 80, 10), 1)
        self.assertEqual(self.k((210, 20, 25, 255), 80, 40), 2)

    def test_other(self):
        self.assertEqual(self.k((30, 40, 60, 255), 80, 10), 3)     # glass
        self.assertEqual(self.k((240, 200, 40, 255), 80, 40), 3)   # the yellow bolt / 95


class Pack(unittest.TestCase):
    def test_layout(self):
        kinds = [4, 0, 0, 0, 0, 2]
        px = [0xF800, 0, 0, 0, 0, 0x07E0]
        b = gm.pack(3, 2, kinds, px, (0, 2, 1, 3, 0, 1))
        self.assertEqual(b[:4], b"CAR1")
        self.assertEqual(struct.unpack("<8H", b[4:20]), (3, 2, 0, 2, 1, 3, 0, 1))
        self.assertEqual(b[20:23], bytes([0x40, 0x00, 0x02]))
        self.assertEqual(struct.unpack("<6H", b[23:]), (0xF800, 0, 0, 0, 0, 0x07E0))
        self.assertEqual(len(b), 20 + 3 + 12)

    def test_rgb565(self):
        self.assertEqual(gm.rgb565(255, 0, 0), 0xF800)
        self.assertEqual(gm.rgb565(0, 255, 0), 0x07E0)
        self.assertEqual(gm.rgb565(255, 255, 255), 0xFFFF)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python tools/test_gen_mcqueen_sprite.py`
Expected: `ModuleNotFoundError: No module named 'gen_mcqueen_sprite'`

- [ ] **Step 3: Write the tool**

```python
# tools/gen_mcqueen_sprite.py
"""Build data/mcqueen.bin: the boot scene's pixel Lightning McQueen (S3 opening C), from a PNG.
Run: python tools/gen_mcqueen_sprite.py <mcqueen.png> [--shoulder 30] [--head 186,24,199,36]
                                       [--tail 0,18,5,30] [--wheels 46,160,52] [--beam 200,31] [--tail-glow 21]

The PNG is a 200 x 66 side view facing right, transparent background, wheels left out (the
scene draws them so they spin, at --wheels). A fan sprite of a Disney/Pixar character: the PNG
and data/mcqueen.bin are git-ignored. Standard library only. Format "CAR1": util/carbin.h.
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gen_status_sprite import decode_png  # noqa: E402

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
OUT = os.path.join(ROOT, "data", "mcqueen.bin")
W, H = 200, 66
NONE, UPPER, LOWER, OTHER, HEAD, TAIL = 0, 1, 2, 3, 4, 5


def inside(box, x, y):
    return box[0] <= x <= box[2] and box[1] <= y <= box[3]


def classify(rgba, x, y, head, tail, shoulder):
    r, g, b, a = rgba
    if a < 128:
        return NONE
    if inside(head, x, y):
        return HEAD
    if inside(tail, x, y):
        return TAIL
    if r >= 140 and g <= 90 and b <= 90:
        return UPPER if y < shoulder else LOWER
    return OTHER


def rgb565(r, g, b):
    return (r & 0xF8) << 8 | (g & 0xFC) << 3 | b >> 3


def pack(w, h, kinds, px, anchors):
    head = b"CAR1" + struct.pack("<8H", w, h, *anchors)
    nib = bytearray((len(kinds) + 1) // 2)
    for i, k in enumerate(kinds):
        nib[i >> 1] |= (k << 4) if i % 2 == 0 else k
    return head + bytes(nib) + struct.pack("<%dH" % len(px), *px)


def box(s):
    return tuple(int(v) for v in s.split(","))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("png")
    ap.add_argument("--shoulder", type=int, default=30)
    ap.add_argument("--head", type=box, default=(186, 24, 199, 36))
    ap.add_argument("--tail", type=box, default=(0, 18, 5, 30))
    ap.add_argument("--wheels", type=box, default=(46, 160, 52))
    ap.add_argument("--beam", type=box, default=(200, 31))
    ap.add_argument("--tail-glow", type=int, default=21)
    a = ap.parse_args()
    w, h, rows = decode_png(open(a.png, "rb").read())
    if (w, h) != (W, H):
        sys.exit("gen_mcqueen_sprite: the PNG must be %d x %d (it is %d x %d)" % (W, H, w, h))
    kinds, px = [], []
    for y in range(h):
        for x in range(w):
            k = classify(rows[y][x], x, y, a.head, a.tail, a.shoulder)
            kinds.append(k)
            px.append(rgb565(*rows[y][x][:3]) if k else 0)
    anchors = (a.wheels[0], a.wheels[1], a.wheels[2], a.beam[0], a.beam[1], a.tail_glow)
    blob = pack(w, h, kinds, px, anchors)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(blob)
    print("gen_mcqueen_sprite: %s  %d bytes, %d opaque pixels" % (os.path.relpath(OUT), len(blob),
                                                                  sum(1 for k in kinds if k)))


if __name__ == "__main__":
    main()
```

- [ ] **Step 4: Run test to verify it passes**

Run: `python tools/test_gen_mcqueen_sprite.py`
Expected: `Ran 6 tests ... OK`

- [ ] **Step 5: Commit**

```bash
git add tools/gen_mcqueen_sprite.py tools/test_gen_mcqueen_sprite.py
git commit -m "feat(s3): pack a pixel McQueen sprite for the boot scene"
```

---

### Task 5: The McQueen art (PixelLab, owner approval)

**Files:**
- Create (git-ignored): `montage-photos/mcqueen.png` (200 × 66 RGBA), `data/mcqueen.bin`

**Interfaces:**
- Consumes: `tools/gen_mcqueen_sprite.py` (Task 4).
- Produces: a `data/mcqueen.bin` that `carbin::parse` accepts, with wheel arches at x 46 and 160 (wheel centre y 52), nose at x ~199, rear at x ~0.

- [ ] **Step 1: Draw him.** Load the PixelLab tools (`ToolSearch` with `select:mcp__pixellab__create_image_pro,mcp__pixellab__get_image,mcp__pixellab__wait_for_jobs,mcp__pixellab__pixelart_workbench,mcp__pixellab__edit_image`). Generate a side view, facing right, transparent background: "Lightning McQueen from Cars, red race car, number 95, yellow lightning bolt decal along the side, cartoon eyes on the windshield, low stock-car body, no wheels (empty wheel arches), pixel art, side profile, night lighting", at 200 × 66 (or larger, then `unzoom_image`/downscale to exactly 200 × 66 with nearest-neighbour). Wheel arches centred at x 46 and 160.
- [ ] **Step 2: Check it against the Honda.** Render both with the night-drive palette: save the PNG next to `python tools/gen_car_sprite.py --png honda.png` in the scratchpad and view them (Read the PNGs). Fix the arch positions or proportions with `pixelart_workbench` or `edit_image`.
- [ ] **Step 3: Owner approval.** Show the owner the McQueen PNG beside the Honda (scaled 3×). Iterate until they approve. Do not continue without approval.
- [ ] **Step 4: Pack it.** Save as `montage-photos/mcqueen.png`; run `python tools/gen_mcqueen_sprite.py montage-photos/mcqueen.png`, tuning `--head` (his headlight decals), `--tail` and `--shoulder` so the lamps light and the paint gets the streetlight reflection. Expected: `gen_mcqueen_sprite: data/mcqueen.bin  33020 bytes, N opaque pixels`.
- [ ] **Step 5: Check the parse on the host.** One-off (scratchpad, not committed):

```bash
python -c "import struct;b=open('data/mcqueen.bin','rb').read();print(b[:4],struct.unpack('<8H',b[4:20]),len(b))"
```

Expected: `b'CAR1' (200, 66, 46, 160, 52, 200, 31, 21) 33020`. Nothing to commit (the files are git-ignored).

---

### Task 6: The on-board terminal as its own module (`ui/bootterm`)

Move the terminal out of `bootscene.cpp` unchanged in behaviour, so the kiss video can show it too.

**Files:**
- Create: `src/ui/bootterm.h`, `src/ui/bootterm.cpp`
- Modify: `src/ui/bootscene.cpp` (remove the terminal code: `TERM_*` constants, `BIOS_*` colours, `biosTone`, `TERM_TEXT`, `s_term`, `s_termDrawn`, `s_termGen`, `termRow`, `termText`, `termDrawDirect`, `termIntoStrip`, `termEvents`, `s_caption`, `s_captionGen`), `src/ui/bootscene.h` (`setCaption` forwards)

**Interfaces:**
- Consumes: `util/bios` (unchanged).
- Produces:

```cpp
namespace bootterm {
constexpr int X = 4, Y = 4, ROW = 10;                       // top left; 10 px a row
constexpr int W = (bios::TERM_COLS + 2) * 6 + 2;           // 206
constexpr int H = bios::TERM_ROWS * ROW;                    // 60
void begin(bool (*online)());       // fresh terminal for a new scene (clears every line and flag)
void setCaption(const char* text);  // static string; "retry" in it turns WI-FI to RETRYING...
const char* caption();
uint32_t captionGen();              // bumps on every setCaption
void events(uint32_t t, bool braking);   // feeds the script and the live lines due by t
uint32_t gen();                     // bumps when any line changed (redraw)
// Rows straight on the display, redrawn only when they change; bg(y) repaints a row's background.
void drawDirect(TFT_eSPI& tft, uint32_t t, uint16_t (*bg)(int y));
// Rows into a sprite whose (0,0) is screen (x0, y0): a strip of the drive, or the video overlay.
void drawInto(TFT_eSprite& spr, uint32_t t, int x0, int y0);
}
```

- [ ] **Step 1: Create `bootterm.cpp`** by moving the code verbatim from `bootscene.cpp` (the `// ---- BIOS colours ----` and `// ---- on-board terminal ----` sections and `termEvents`), with these changes:
  - `TERM_X/TERM_Y/TERM_ROW/TERM_W` → `X/Y/ROW/W`.
  - `termEvents`' function-local statics (`online, engine, greet, done, parked`) become file-scope variables that `begin()` resets, together with `s_term = {}`, `s_termGen = 0` and every `s_termDrawn[i] = -1`.
  - `termDrawDirect(t)` → `drawDirect(tft, t, bg)`: `s_tft->` → `tft.`, `skyAt(yy)` → `bg(yy)`.
  - `termIntoStrip(spr, t)` → `drawInto(spr, t, x0, y0)`: skip rows with `y >= y0 + spr.height() || y + 8 <= y0`; draw at `(x - x0, y - y0)`.
  - `UNDERGLOW` (for `bios::PURPLE`) is `rgb565::hex(0x8C46FF)`: define it locally in `bootterm.cpp`.
  - `s_online` and `s_caption`/`s_captionGen` live here now.
- [ ] **Step 2: Use it in `bootscene.cpp`.** At the scene task's start: `bootterm::begin(s_online);`. Replace calls: `termEvents(t, braking)` → `bootterm::events(t, braking)`; `termDrawDirect(t)` → `bootterm::drawDirect(*s_tft, t, skyAt)`; `termIntoStrip(spr, t)` → `bootterm::drawInto(spr, t, 0, s_y0)`; `starSkipped` uses `bootterm::X/Y/W/ROW`. `runWithoutScene` reads `bootterm::caption()` / `bootterm::captionGen()`. `bootscene::setCaption(text)` becomes `bootterm::setCaption(text)`.
- [ ] **Step 3: Build and compare.** `pio run -e s3`, `pio run -e cyd` → `SUCCESS`; `tools/run_tests.ps1` → `fail=0`. Flash the S3 and boot once with the Honda (force it: Task 10's `INTRO_FORCE` doesn't exist yet, so just boot until the Honda drive comes up, or boot with WiFi off to watch `RETRYING...`). The terminal types the same lines as before.
- [ ] **Step 4: Commit**

```bash
git add src/ui/bootterm.* src/ui/bootscene.*
git commit -m "refactor(boot): the on-board terminal as its own module"
```

---

### Task 7: Compositing the terminal into a video block (`util/termblend`)

**Files:**
- Create: `src/util/termblend.h`, `src/util/termblend.cpp`
- Test: `test/test_termblend/test_termblend.cpp`

**Interfaces:**
- Consumes: `rgb565::be`, `bootanim::scale565`.
- Produces:

```cpp
namespace termblend {
constexpr uint16_t KEY = 0x0120;     // the overlay's "no glyph" colour (big-endian, a dark green no text uses)
constexpr uint8_t SHADE = 100;       // panel brightness under the text: ~40 % of the video
// block: bw x bh big-endian RGB565 at screen (bx, by), modified in place. overlay: ow x oh at
// (ox, oy), big-endian, KEY where there's no glyph. Pixels under the overlay are darkened to
// SHADE; glyph pixels replace them. Pixels outside the overlay are untouched.
void composite(uint16_t* block, int bx, int by, int bw, int bh,
               const uint16_t* overlay, int ox, int oy, int ow, int oh);
}
```

- [ ] **Step 1: Write the failing test**

```cpp
// test/test_termblend/test_termblend.cpp
#include <unity.h>
#include <vector>
#include "../../src/util/termblend.h"
#include "../../src/util/rgb565.h"
#include "../../src/util/bootanim.h"

void setUp() {}
void tearDown() {}

static const uint16_t VIDEO = rgb565::be(0xFFFF);   // a white frame, big-endian like JPEGDEC's output
static const uint16_t GLYPH = rgb565::be(0x07E0);

// A 4x2 overlay at (10, 20): glyph at (11, 20), KEY elsewhere.
static std::vector<uint16_t> overlay() {
    std::vector<uint16_t> o(8, termblend::KEY);
    o[1] = GLYPH;
    return o;
}

void test_glyph_replaces_and_panel_darkens() {
    std::vector<uint16_t> b(8 * 4, VIDEO);              // block 8x4 at (8, 18)
    const std::vector<uint16_t> o = overlay();
    termblend::composite(b.data(), 8, 18, 8, 4, o.data(), 10, 20, 4, 2);
    TEST_ASSERT_EQUAL_HEX16(GLYPH, b[(20 - 18) * 8 + (11 - 8)]);
    const uint16_t dark = rgb565::be(bootanim::scale565(0xFFFF, termblend::SHADE));
    TEST_ASSERT_EQUAL_HEX16(dark, b[(20 - 18) * 8 + (10 - 8)]);
    TEST_ASSERT_EQUAL_HEX16(dark, b[(21 - 18) * 8 + (13 - 8)]);
}

// Review focus 5: a block half over the panel changes only the overlapping pixels.
void test_block_partly_over_the_panel() {
    std::vector<uint16_t> b(8 * 4, VIDEO);
    const std::vector<uint16_t> o = overlay();
    termblend::composite(b.data(), 8, 18, 8, 4, o.data(), 10, 20, 4, 2);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 8; ++x) {
            const int sx = 8 + x, sy = 18 + y;
            const bool in = sx >= 10 && sx < 14 && sy >= 20 && sy < 22;
            if (!in) TEST_ASSERT_EQUAL_HEX16(VIDEO, b[y * 8 + x]);
            else TEST_ASSERT_NOT_EQUAL(VIDEO, b[y * 8 + x]);
        }
}

void test_block_outside_is_untouched() {
    std::vector<uint16_t> b(16 * 16, VIDEO);
    const std::vector<uint16_t> o = overlay();
    termblend::composite(b.data(), 100, 100, 16, 16, o.data(), 10, 20, 4, 2);   // below right
    termblend::composite(b.data(), 0, 0, 8, 8, o.data(), 10, 20, 4, 2);         // above left (8x8 view)
    for (uint16_t v : b) TEST_ASSERT_EQUAL_HEX16(VIDEO, v);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_glyph_replaces_and_panel_darkens);
    RUN_TEST(test_block_partly_over_the_panel);
    RUN_TEST(test_block_outside_is_untouched);
    return UNITY_END();
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/ntest.ps1 test/test_termblend/test_termblend.cpp`
Expected: COMPILE FAILED (`termblend.h` missing).

- [ ] **Step 3: Write the implementation**

```cpp
// src/util/termblend.h
#pragma once
#include <cstdint>
// The boot terminal over the kiss video (ui/intro): composited into each JPEG block as it's
// decoded, so the video never wipes it and there's no second pass. PURE, host-tested.
namespace termblend {
constexpr uint16_t KEY = 0x0120;     // overlay "no glyph" (big-endian 0x2001: a green no text uses)
constexpr uint8_t SHADE = 100;       // the panel: the video at ~40 % under the text
void composite(uint16_t* block, int bx, int by, int bw, int bh,
               const uint16_t* overlay, int ox, int oy, int ow, int oh);
}
```

```cpp
// src/util/termblend.cpp
#include "termblend.h"
#include "rgb565.h"
#include "bootanim.h"

namespace termblend {

void composite(uint16_t* block, int bx, int by, int bw, int bh,
               const uint16_t* overlay, int ox, int oy, int ow, int oh) {
    const int x0 = bx > ox ? bx : ox, x1 = (bx + bw) < (ox + ow) ? bx + bw : ox + ow;
    const int y0 = by > oy ? by : oy, y1 = (by + bh) < (oy + oh) ? by + bh : oy + oh;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            uint16_t& p = block[(y - by) * bw + (x - bx)];
            const uint16_t g = overlay[(y - oy) * ow + (x - ox)];
            p = g != KEY ? g : rgb565::be(bootanim::scale565(rgb565::be(p), SHADE));
        }
}

}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `powershell -NoProfile -ExecutionPolicy Bypass -File tools/ntest.ps1 test/test_termblend/test_termblend.cpp src/util/termblend.cpp src/util/bootanim.cpp`
Expected: `3 Tests 0 Failures 0 Ignored`

- [ ] **Step 5: Commit**

```bash
git add src/util/termblend.* test/test_termblend
git commit -m "feat(s3): composite the boot terminal into a video block"
```

---

### Task 8: The kiss video shows the terminal and holds for WiFi (`ui/intro`)

**Files:**
- Modify: `src/ui/intro.h`, `src/ui/intro.cpp`

**Interfaces:**
- Consumes: `bootterm::begin/events/drawInto/gen`, `bootterm::X/Y/W/H` (Task 6); `termblend::composite`, `termblend::KEY` (Task 7).
- Produces: `void intro::start(TFT_eSPI& t, bool (*online)());` (was `start(TFT_eSPI&)`); `bool intro::available();` (a clip is in the build); `wait()` now returns once the clip is over **and** `online()` is true.

- [ ] **Step 1: The overlay sprite and the callback.** In `intro.cpp` (S3 part):

```cpp
#include "bootterm.h"
#include "../util/termblend.h"

static bool (*s_online)() = nullptr;
static TFT_eSprite* s_term = nullptr;   // bootterm::W x H, KEY where there's no glyph
static uint16_t* s_termPx = nullptr;
static uint32_t s_termGen = 0xFFFFFFFFu, s_t0 = 0;

// The terminal's pixels for this frame: redrawn into the sprite only when a line changed or types on.
static void renderTerm(uint32_t t) {
    bootterm::events(t, false);
    s_term->fillSprite(rgb565::be(termblend::KEY));   // fillSprite takes native; KEY is big-endian
    bootterm::drawInto(*s_term, t, bootterm::X, bootterm::Y);
}

static int draw(JPEGDRAW* d) {
    if (s_termPx)
        termblend::composite((uint16_t*)d->pPixels, d->x, d->y, d->iWidth, d->iHeight,
                             s_termPx, bootterm::X, bootterm::Y, bootterm::W, bootterm::H);
    s_tft->pushImage(d->x, d->y, d->iWidth, d->iHeight, d->pPixels);
    return 1;
}
```

`#include "../util/rgb565.h"` too. Check after building that the sprite's buffer is big-endian like JPEGDEC's `RGB565_BIG_ENDIAN` output (TFT_eSprite stores 16-bit pixels byte-swapped; `bootscene` relies on the same). If the glyphs come out with swapped colours on the deck, drop the `rgb565::be` around `KEY` in `fillSprite` and compare `g != rgb565::be(KEY)` instead — verify by eye in Step 4.

- [ ] **Step 2: Decode with the overlay, then hold.** Factor the decode into `static void show(int i)` (open, decode, close, as today). In `task`: before each `show(i)`, call `renderTerm(millis() - s_t0)`. After the last frame, hold:

```cpp
    // Not online yet: keep the last frame up with the terminal (RETRYING...), redrawn at 5 fps.
    while (!s_online()) {
        renderTerm(millis() - s_t0);
        show(montage::count() - 1);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
```

In `start(t, online)`: set `s_online = online`, `s_t0 = millis()`, `bootterm::begin(online)`, then create the sprite before the task:

```cpp
    s_term = new TFT_eSprite(&t);
    s_term->setColorDepth(16);
    s_termPx = (uint16_t*)s_term->createSprite(bootterm::W, bootterm::H);   // ~25 KB; no memory = no terminal
```

At the task's end (before `s_playing = false`): `if (s_term) { s_term->deleteSprite(); delete s_term; s_term = nullptr; } s_termPx = nullptr;`.

Update the CYD stubs in the `#else` part: `void start(TFT_eSPI&, bool (*)()) {}`, `bool available() { return false; }`.

Add `bool available()` (parses `intro.bin` header: `montage::parse(...) && montage::count() > 0`, as `start` does) and declare it in `intro.h`, and update the header comment: the terminal runs over the video; `wait()` returns once the clip is over and the deck is online.

- [ ] **Step 3: Log the cost.** Keep the existing `[intro] %d of %d frames in %lu ms (%lu ms a frame)` line and add the hold: `[intro] held %lu ms for WiFi`.
- [ ] **Step 4: On the deck.** Temporarily call `intro::start(s_tft, net::isOnline)` as today in `main.cpp` (Task 10 rewires it properly). Flash, boot: the terminal types over the kiss, readable; the `[intro]` line shows how many frames played. **Budget rule (spec):** if frames shown drop by more than ~5 % of the clip versus today (439–443 of 462), render the terminal into the sprite only every other frame (`if (i & 1) renderTerm(...)`) and re-measure. Boot once with the hotspot off: the last frame holds with `WI-FI ... RETRYING...` until the hotspot comes back.
- [ ] **Step 5: Commit**

```bash
git add src/ui/intro.* src/main.cpp
git commit -m "feat(s3): the boot terminal over the kiss video, holding for WiFi"
```

---

### Task 9: The night drive with a chosen car, or straight to the montage (`ui/bootscene`)

**Files:**
- Modify: `src/ui/bootscene.h`, `src/ui/bootscene.cpp`

**Interfaces:**
- Consumes: `carlook::Look` (Task 3).
- Produces: `struct bootscene::Opts { carlook::Look car; bool drive; };` and `void bootscene::start(TFT_eSPI& tft, bool (*online)(), const bootscene::Opts& o);`. The old `start(tft, online)` stays as `start(tft, online, {carlook::honda(), true})` (the CYD's call).

- [ ] **Step 1: Options.** In `bootscene.h`:

```cpp
#include "carlook.h"
// car: which car drives (ui/carlook); drive false: no drive, the montage starts once online()
// (the kiss opening: the video already ran).
struct Opts { carlook::Look car; bool drive; };
void start(TFT_eSPI& tft, bool (*online)(), const Opts& o);
void start(TFT_eSPI& tft, bool (*online)());   // the Honda's drive
```

In `bootscene.cpp`: `Opts s_opts{carlook::honda(), true};` file-scope; `start(tft, online, o)` stores it and sets `s_car = o.car`; the two-argument `start` calls `start(tft, online, Opts{carlook::honda(), true})`.

- [ ] **Step 2: No drive.** In `task`, before `drawSky()`: when `!s_opts.drive`, don't draw the sky or the terminal. In the loop:

```cpp
            if (!s_opts.drive && tHero == bootanim::NO_EXIT) {
                if (!s_online()) { vTaskDelayUntil(&wake, pdMS_TO_TICKS(FRAME_MS)); continue; }
                tHero = t;
                skipBrake = true;                        // straight to the montage
            }
            const uint32_t u = tHero == bootanim::NO_EXIT ? 0
                             : t - tHero + (skipBrake ? bootanim::BRAKE_MS : 0);
```

with `bool skipBrake = false;` declared beside `tHero`. The existing `mayExit` branch only runs when `s_opts.drive`. `termEvents`/`termDrawDirect` calls are skipped when `!s_opts.drive` (the montage covers the screen).
- [ ] **Step 3: Build** `pio run -e s3` and `pio run -e cyd` → `SUCCESS`; `tools/run_tests.ps1` → `fail=0`.
- [ ] **Step 4: Commit**

```bash
git add src/ui/bootscene.*
git commit -m "feat(s3): the boot scene drives a chosen car, or goes straight to the montage"
```

---

### Task 10: The boot flow (`main.cpp`, S3)

**Files:**
- Modify: `src/main.cpp` (`setup()`'s `#ifdef BOARD_S3` branch), `platformio.ini` (comment for `INTRO_FORCE`)

**Interfaces:**
- Consumes: `intropick::pick/forced/parse/name/greetingFor` (Task 1), `greeting::choose` (Task 2), `carlook::honda/mcqueen` (Task 3), `intro::available/start(t, online)/wait` (Task 8), `bootscene::start(t, online, Opts)` (Task 9), `bootterm::setCaption` (Task 6).

- [ ] **Step 1: Pick and remember the opening.** Above `setup()`:

```cpp
#ifdef BOARD_S3
#include <Preferences.h>
#include "util/intropick.h"
#include "ui/carlook.h"
// This boot's opening (util/intropick): random, never last boot's (NVS "boot"/"last"), only
// from what the build has. -D INTRO_FORCE=\"kiss\" (or honda, mcqueen) forces one for testing.
static intropick::Opening pickOpening(bool haveKiss, bool haveMcQueen) {
    Preferences p;
    p.begin("boot", false);
    const int last = p.getInt("last", -1);
    int o = -1;
#ifdef INTRO_FORCE
    o = intropick::forced(intropick::parse(INTRO_FORCE), haveKiss, haveMcQueen);
#endif
    if (o < 0) o = intropick::pick(esp_random(), last, haveKiss, haveMcQueen);
    p.putInt("last", o);
    p.end();
    Serial.printf("[boot] opening: %s\n", intropick::name((intropick::Opening)o));
    return (intropick::Opening)o;
}
#endif
```

- [ ] **Step 2: The S3 sequence.** Replace the S3 branch from `intro::start(s_tft);` to the end of the `if (wifi) ... else ...` with:

```cpp
    carlook::Look mcqueen;
    const bool haveMcQueen = carlook::mcqueen(&mcqueen);
    const intropick::Opening opening = pickOpening(intro::available(), haveMcQueen);
    greeting::choose(intropick::greetingFor(opening));
    if (opening == intropick::KISS) intro::start(s_tft, net::isOnline);
    const bool wifi = net::connectAny();
    if (wifi) {
        mem::log("boot+wifi");
        netclock::begin();                // the clock (night palette) right away, not after login
        nettask::start(false, nullptr);   // logs in, then polls; no TLS hold
    } else {
        bootterm::setCaption("WiFi not found - retrying...");   // the terminal says RETRYING...
        nettask::start(false, nullptr);   // keeps retrying WiFi underneath
    }
    intro::wait();                        // kiss: over, and online (the last frame holds till then)
    greeting::play();                     // the opening's greeting (none for the kiss)
    const bootscene::Opts opts{opening == intropick::MCQUEEN ? mcqueen : carlook::honda(),
                               opening != intropick::KISS};
    bootscene::start(s_tft, net::isOnline, opts);
    if (wifi) bootscene::setCaption("Connecting to Spotify...");
    bootscene::waitDone();                // the display is ours again
```

Remove the now-unused `bootscene::setCaption("WiFi not found - retrying...")` duplicate in the old `else` branch. Check the `nettask::start` contract (`src/core/nettask.h`): it must be called exactly once; both branches call it once. Read the old `else` branch's comment: the scene used to keep running while the net task retried — the new flow does the same (`bootscene` waits for `online()` before the car may exit, the kiss waits in `intro::wait()`).

- [ ] **Step 3: Document the flag.** In `platformio.ini`, `[env:s3]`, above `build_flags`:

```ini
; Testing the boot openings: add  -D INTRO_FORCE=\"kiss\"  (or honda, mcqueen) to build_flags.
```

- [ ] **Step 4: Build and boot each opening.** `pio run -e s3`, `pio run -e cyd` → `SUCCESS`. Flash three times with `INTRO_FORCE` set to each opening (then remove it), capturing serial with `python tools/capture_serial.py COM12 60 .superpowers/open_<name>.log --no-reset` right after the flash. Each log shows `[boot] opening: <name>`; McQueen drives with "eu sou a velocidade"; the Honda with "bem vindo moreno"; the kiss plays its sound only. Then flash without the flag and reset 3 times: three different consecutive openings never repeat back to back.
- [ ] **Step 5: Commit**

```bash
git add src/main.cpp platformio.ini
git commit -m "feat(s3): boot with a random opening, then the montage"
```

---

### Task 11: The kiss cut (`tools/gen_intro.py`)

**Files:**
- Modify: `tools/gen_intro.py` (`DEFAULT_SEGMENTS`, docstring), `README.md` (intro row)
- Rebuild (git-ignored): `data/intro.bin`, `data/intro.pcm`

- [ ] **Step 1: Find the seconds.** Extract one frame a second from the clip into the scratchpad and look at them (Read the JPGs):

```bash
ffmpeg -loglevel error -i montage-photos/sabrina-carpenter-kissing-screen-live-wallpaper.mp4 -vf fps=1,scale=160:-1 <scratchpad>/kiss_%02d.jpg
```

Pick START = the first frame where her shadow appears on the wall, END = just after the kiss lands (the lips on the wall), so END − START ≈ 10 s. Refine to 0.1 s with `-ss`/`-t` frame grabs if needed.
- [ ] **Step 2: Make it the default.** `DEFAULT_SEGMENTS = [(START, END)]`, and rewrite the docstring's "Default" sentence: "Default, for the kiss wallpaper: the walk-in and the kiss, START–END s (~10 s)."
- [ ] **Step 3: Rebuild.** `python tools/gen_intro.py montage-photos/sabrina-carpenter-kissing-screen-live-wallpaper.mp4`. Expected: a ~10 s clip, ~200 frames, well under `MAX_BYTES`.
- [ ] **Step 4: Run the tool tests** `python -m unittest discover -s tools -p "test_gen_*.py"` → `OK`.
- [ ] **Step 5: Commit**

```bash
git add tools/gen_intro.py README.md
git commit -m "feat(s3): the kiss intro is the walk-in and the kiss (~10 s)"
```

---

### Task 12: The shorter montage and "POWERED BY HONDA" (`tools/gen_montage.py`)

**Files:**
- Modify: `tools/gen_montage.py` (`SHOTS`, `DETAIL`, `END_*`, `end_card`)
- Test: `tools/test_gen_montage.py` (create)
- Rebuild (git-ignored): `data/montage.bin`

**Interfaces:**
- Produces: `photo_frames() -> int` (frames of the photo part after the crossfades) and `end_filter(...) -> str` (the end card's ffmpeg filter, now with `drawtext`), both importable for the test.

- [ ] **Step 1: Write the failing test**

```python
# tools/test_gen_montage.py
"""Host tests for tools/gen_montage.py (no ffmpeg needed). Run: python tools/test_gen_montage.py"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_montage as gm  # noqa: E402


class Length(unittest.TestCase):
    def test_photos_take_about_five_seconds(self):
        s = gm.photo_frames() / gm.FPS
        self.assertGreaterEqual(s, 4.5)
        self.assertLessEqual(s, 5.5)

    def test_end_card_about_two_and_a_half_seconds(self):
        s = (gm.END_IN + gm.END_HOLD + gm.END_OUT) / gm.FPS
        self.assertGreaterEqual(s, 2.0)
        self.assertLessEqual(s, 3.0)


class EndCard(unittest.TestCase):
    def test_powered_by_honda_in_red(self):
        f = gm.end_filter(100, 40, 0, 0)
        self.assertIn("drawtext", f)
        self.assertIn("POWERED BY HONDA", f)
        self.assertIn(gm.END_RED, f)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run it to verify it fails**

Run: `python tools/test_gen_montage.py`
Expected: `AttributeError: module 'gen_montage' has no attribute 'photo_frames'`

- [ ] **Step 3: Shorten and add the text**

```python
SHOTS = [("07-right-wide-turning.jpg", 325, 603, 610, 0.95, 14, 0.0019, 0),
         ("02-front-close.jpg", 282, 655, 556, 0.87, 11, 0.005, -1),
         ("04-left-closer.jpg", 291, 653, 570, 0.89, 12, 0.005, -1)]
DETAIL = ("06-headlight-detail.jpg", 290, 18, 0.0045)       # plain crop at y, frames, push-in
END_IN, END_HOLD, END_OUT = 4, 16, 5                        # frames (the held frames share one JPEG)
END_TEXT, END_TEXT_SIZE = "POWERED BY HONDA", 15
END_FONT = "C\\:/Windows/Fonts/arialbd.ttf"                 # ffmpeg drawtext path (Windows)


def photo_frames():
    """Frames of the photo part: every shot and the detail, minus the crossfade overlaps."""
    n = [s[5] for s in SHOTS] + [DETAIL[2]]
    return sum(n) - XFADE * (len(n) - 1)


def end_filter(cw, ch, x0, y0, w=1, h=1):
    """The end card: the logo (w x h source, cropped cw x ch at x0, y0) on white, raised for the
    text, the red frame, POWERED BY HONDA in the frame's red, the fades."""
    flat = "color=white:s=%dx%d[w];[w][0]overlay=shortest=1" % (w, h)
    return (flat + ",crop=%d:%d:%d:%d,scale=%d:-1:flags=area[l];[1][l]overlay=(W-w)/2:(H-h)/2-16,"
            "drawbox=x=0:y=0:w=320:h=240:color=%s:t=%d,"
            "drawtext=fontfile='%s':text='%s':fontsize=%d:fontcolor=%s:x=(w-text_w)/2:y=h-46,"
            "fade=t=in:s=0:n=%d:color=white,fade=t=out:s=%d:n=%d,format=yuvj420p"
            % (cw, ch, x0, y0, END_LOGO_W, END_RED, END_BORDER, END_FONT, END_TEXT, END_TEXT_SIZE,
               END_RED, END_IN, END_IN + END_HOLD, END_OUT - 1))
```

In `end_card`, keep the `flat` line (the gray probe still uses it) and replace the inline `-filter_complex` string of the second `ffmpeg([...])` call with `end_filter(cols[-1] - x0 + 1, rows[-1] - y0 + 1, x0, y0, w, h)`. Update the module docstring ("~5 s of photos, then the end card with POWERED BY HONDA").

Check `0xE2001A` works as `fontcolor` (ffmpeg accepts `0xRRGGBB`); if the test's `assertIn(gm.END_RED, f)` passes, it's in the string twice (drawbox and drawtext).

- [ ] **Step 4: Run the tests**

Run: `python tools/test_gen_montage.py` → `OK`, and `python -m unittest discover -s tools -p "test_gen_*.py"` → `OK`.

- [ ] **Step 5: Rebuild and look at it.** `python tools/gen_montage.py` (needs ffmpeg). Expected: `gen_montage: data/montage.bin  ~71 frames, ~7.1 s`. Extract the end card's held frame and view it (the logo, the red frame, POWERED BY HONDA readable, nothing clipped):

```bash
python -c "import struct;b=open('data/montage.bin','rb').read();n=struct.unpack('<H',b[4:6])[0];o,l=struct.unpack('<II',b[8+8*(n-8):16+8*(n-8)]);open('<scratchpad>/end.jpg','wb').write(b[o:o+l])"
```

- [ ] **Step 6: Commit**

```bash
git add tools/gen_montage.py tools/test_gen_montage.py
git commit -m "feat(boot): a ~5 s montage and a POWERED BY HONDA end card"
```

---

### Task 13: Docs and the whole boot on the deck

**Files:**
- Modify: `README.md` (features: the random opening; the assets table: McQueen row, montage and intro rows), `docs/ARCHITECTURE.md` (boot section: intropick, carlook/carbin, bootterm, termblend, intro hold), `CHANGELOG.md` (Added: random opening, McQueen; Changed: shorter boot, montage, end card), `CREDITS.md` (fan McQueen sprite, Disney/Pixar, not in the repo)

- [ ] **Step 1: Write the docs** in the files' existing style (short bullets, owner-facing).
- [ ] **Step 2: Full verification.**
  - `powershell -NoProfile -ExecutionPolicy Bypass -File tools/run_tests.ps1` → `fail=0`
  - `python -m unittest discover -s tools -p "test_gen_*.py"` → `OK`
  - `pio run -e s3`, `pio run -e cyd`, `pio run -e cyd_companion` → `SUCCESS`
- [ ] **Step 3: Time each opening on the deck.** Flash; for each opening (force with `INTRO_FORCE`, then remove it), power-cycle and capture serial from the start (`python tools/capture_serial.py COM12 60 ... --no-reset` started before plugging in, or press RESET with the capture running). Read the time between the first log line and `[bootscene] ... ms` / the first `[screen]` or deck draw. Expected: kiss ~19 s, Honda ~16 s, McQueen ~15 s, all within 15–25 s. Note them in the CHANGELOG entry.
- [ ] **Step 4: Commit**

```bash
git add README.md docs/ARCHITECTURE.md CHANGELOG.md CREDITS.md
git commit -m "docs: the random boot opening, McQueen and the shorter montage"
```
