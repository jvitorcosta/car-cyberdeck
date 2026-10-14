# Avatar Home Screen Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** On the old deck (the CYD companion), the avatar is the home screen. She's shown at 2× next to a battle command box (LYRICS / SOUNDS / TRIP), the screens are touch-driven, and a message bar carries her lines and reactions.

**Architecture:**
- **Pure, host-tested modules:**
  - `util/press`: tap and hold tracking;
  - `util/menunav`: screens, buttons, hit test, navigation, idle text;
  - `util/touchmap`: raw touch → screen coordinates;
  - `util/avatarlines`: gains reaction events.
- **Device side:**
  - `companion/touch`: the XPT2046 on the free VSPI bus;
  - `companion/link`: gains an action queue (POST `/play`, `/trip/reset`);
  - `companion/avatarview`: draws her at 1× or 2× wherever the screen puts her;
  - `companion/screens`: draws Home, Sounds, Trip, the buttons and the message bar;
  - `companion/dashboard`: becomes the Lyrics screen's content;
  - `companion/app`: runs the screen state machine.

**Tech Stack:**
- PlatformIO espressif32@6.9.0, arduino-esp32 2.0.17, C++17, TFT_eSPI 2.5.43.
- PaulStoffregen XPT2046_Touchscreen `f956c5d`, already used by `hwcheck`.
- Unity host tests via `tools/run_tests.ps1`.

**Spec:** `docs/superpowers/specs/2026-10-14-avatar-home-design.md`. It builds on `docs/superpowers/specs/2026-10-14-avatar-design.md`.

## Global Constraints

- **Branch:** keep working on `feat/avatar`. Commits so far go up to `c5eca48`, with the avatar on the companion beside the lyrics (Task 7R of the previous plan).
- **Builds:** only `cyd_companion` changes behaviour. The `s3` and `cyd` firmware must build unchanged: `s3` 3 004 913 bytes, `cyd` 3 034 621 bytes. CI checks the CYD at 97 %.
- **Committed vs. local:** the character art stays git-ignored. Tests use made-up names and lines.
- **Screen:** 320×240, rotation 1, theme palette; the night palette repaints the current screen.
- **Message bar:** y 206–239, a dialogue box `{4, 206, 312, 34}`.
- **Touch:**
  - a button is chosen at touch-down;
  - normal buttons fire on release inside the button;
  - RESET fires once after **1000 ms** held inside;
  - sliding onto another button never fires it;
  - pressure must be ≥ **400**.
- **Calibration constants:** `TOUCH_RAW_X0/X1/Y0/Y1`, defaults 200/3900/200/3900.
- **Actions:**
  - `POST http://<COMPANION_DECK>/play?clip=howl|vroom&k=<REMOTE_TOKEN>`;
  - `POST /trip/reset?k=<REMOTE_TOKEN>`;
  - 2 s timeouts;
  - one slot (a new request replaces one still waiting).
- **Without an avatar file:** the companion behaves exactly as before this branch (full-screen lyrics with its status strip), and touch is ignored.

**Plan rulings (deviations from the spec, decided while planning):**
1. **The touch controller uses the VSPI bus, not HSPI.** On the CYD, the display owns HSPI (`platformio.ini`), and the SD slot that normally uses VSPI isn't used by the companion. Sharing HSPI would reroute the display's pins.
2. **Lyrics screen: BACK sits at y 110, not y 104.** Her name tag under the 1× portrait occupies y 98–106.
3. **A fourth action result, Busy.** The new deck refuses with 409 (busy), 429 (too soon) or 503 (low memory). That's not "can't reach", so it gets its own reaction event and line ("One at a time, darling!").
4. **Calibration through the companion's own log.** The companion logs `[touch] raw x=.. y=.. -> sx,sy` at each touch-down. Calibrating no longer needs a separate `hwcheck` flash; the constants are still set from those raw values.

## Review Focus

1. **A resistive-touch bounce:** the finger lifts for one 20 ms sample in the middle of a press. Expected: at worst one tap fires, never two. (Task 1: `test_bounce_is_a_new_press_not_two_fires_of_one`.)
2. **Holding RESET, sliding off, then sliding back:** expected: it fires only after a full second back on RESET, not at once. (Task 1: `test_hold_restarts_after_sliding_back`.)
3. **HOWL pressed again while a howl is still playing:** expected: she says "One at a time, darling!", not "I can't reach the deck". (Task 3 events; Task 5 maps 409/429/503 to Busy.)
4. **The new deck starts or stops offering VROOM while the Sounds screen is open:** expected: the screen repaints with or without the button, and never fires a button that isn't drawn. (Task 2: `test_vroom_only_when_offered`; Task 7 repaints on change.)
5. **Touch with no avatar file:** expected: ignored, with today's full-screen lyrics. (Task 7: the no-avatar path returns before reading touch.)

---

## File structure

| File | Responsibility |
|---|---|
| `src/util/press.{h,cpp}` | Tap and hold tracking. Pure. |
| `src/util/menunav.{h,cpp}` | Screens, buttons and rectangles, hit test, navigation, hold flag, idle message text. Pure. |
| `src/util/touchmap.h` | Raw XPT2046 → screen coordinates, with calibration. Pure, header-only. |
| `src/util/avatarlines.{h,cpp}` | + Howl, Vroom, TripReset, Busy, Unreachable reactions. |
| `include/pins.h` | + touch calibration constants. |
| `src/companion/touch.{h,cpp}` | XPT2046 on VSPI; `read(&x, &y)`; logs raw values at touch-down. |
| `src/companion/link.{h,cpp}` | + `request(Action)` and `takeResult(&action)`; POSTs in the link task. |
| `src/companion/avatarview.{h,cpp}` | `place(x, y, scale)`, 1× or 2× drawing, `say(event)`, `update()` returns her line for the bar. |
| `src/companion/screens.{h,cpp}` | Draws each screen's background and buttons, the button states, the message bar and the trip time. |
| `src/companion/dashboard.{h,cpp}` | The Lyrics screen's lyric column (with the avatar) or today's full screen (without). |
| `src/companion/app.cpp` | Screen state, touch → press → navigation / action, results → her reactions. |
| `platformio.ini` | `cyd_companion`: XPT2046 library. |
| `test/test_press/`, `test/test_menunav/`, `test/test_touchmap/` | New suites. `test/test_avatarlines/` is extended. |

---

### Task 1: `util/press`: tap and hold

**Files:**
- Create: `src/util/press.h`, `src/util/press.cpp`
- Test: `test/test_press/test_press.cpp`

**Interfaces:**
- Produces:
  - `press::HOLD_MS` (1000);
  - `class press::Press { int update(bool down, int under, bool hold, uint32_t nowMs); float holdProgress(uint32_t nowMs) const; int pressed() const; }`.
  - Button ids are plain ints, and 0 means none.

- [ ] **Step 1: Write the failing test**

`test/test_press/test_press.cpp`:

```cpp
#include <unity.h>
#include "../../src/util/press.h"

void setUp() {}
void tearDown() {}

void test_tap_fires_on_release_inside() {
    press::Press p;
    TEST_ASSERT_EQUAL_INT(0, p.update(true, 3, false, 0));
    TEST_ASSERT_EQUAL_INT(3, p.pressed());
    TEST_ASSERT_EQUAL_INT(0, p.update(true, 3, false, 60));
    TEST_ASSERT_EQUAL_INT(3, p.update(false, 0, false, 120));
    TEST_ASSERT_EQUAL_INT(0, p.pressed());
}
void test_sliding_out_cancels_the_tap() {
    press::Press p;
    p.update(true, 3, false, 0);
    p.update(true, 0, false, 60);                       // finger moved off the button
    TEST_ASSERT_EQUAL_INT(0, p.update(false, 0, false, 120));
}
void test_sliding_onto_another_button_fires_nothing() {
    press::Press p;
    p.update(true, 3, false, 0);
    p.update(true, 5, false, 60);
    TEST_ASSERT_EQUAL_INT(0, p.update(false, 0, false, 120));
}
void test_down_on_nothing_fires_nothing() {
    press::Press p;
    p.update(true, 0, false, 0);
    p.update(true, 3, false, 60);                       // sliding onto a button later
    TEST_ASSERT_EQUAL_INT(0, p.update(false, 0, false, 120));
}
void test_hold_fires_once_at_one_second() {
    press::Press p;
    p.update(true, 7, true, 1000);
    TEST_ASSERT_EQUAL_INT(0, p.update(true, 7, true, 1999));
    TEST_ASSERT_EQUAL_INT(7, p.update(true, 7, true, 2000));
    TEST_ASSERT_EQUAL_INT(0, p.update(true, 7, true, 2500));    // still held: not again
    TEST_ASSERT_EQUAL_INT(0, p.update(false, 0, false, 2600));  // and no tap on release
}
void test_short_tap_on_a_hold_button_does_nothing() {
    press::Press p;
    p.update(true, 7, true, 0);
    p.update(true, 7, true, 999);
    TEST_ASSERT_EQUAL_INT(0, p.update(false, 0, false, 1000));
}
// Review Focus 2
void test_hold_restarts_after_sliding_back() {
    press::Press p;
    p.update(true, 7, true, 0);
    p.update(true, 0, true, 600);                       // slid off
    p.update(true, 7, true, 700);                       // back on: the second starts again
    TEST_ASSERT_EQUAL_INT(0, p.update(true, 7, true, 1600));
    TEST_ASSERT_EQUAL_INT(7, p.update(true, 7, true, 1700));
}
void test_hold_progress() {
    press::Press p;
    p.update(true, 7, true, 0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, p.holdProgress(500));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, p.holdProgress(3000));
    p.update(true, 0, true, 600);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, p.holdProgress(600));   // off the button
    press::Press q;
    q.update(true, 3, false, 0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, q.holdProgress(500));   // not a hold button
}
// Review Focus 1: a lift for one sample ends the press; the next touch is a new press.
void test_bounce_is_a_new_press_not_two_fires_of_one() {
    press::Press p;
    p.update(true, 3, false, 0);
    TEST_ASSERT_EQUAL_INT(3, p.update(false, 0, false, 20));      // the bounce: one tap
    TEST_ASSERT_EQUAL_INT(0, p.update(true, 3, false, 40));       // a new press starts
    TEST_ASSERT_EQUAL_INT(0, p.update(true, 3, false, 60));       // nothing fires while held
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_tap_fires_on_release_inside);
    RUN_TEST(test_sliding_out_cancels_the_tap);
    RUN_TEST(test_sliding_onto_another_button_fires_nothing);
    RUN_TEST(test_down_on_nothing_fires_nothing);
    RUN_TEST(test_hold_fires_once_at_one_second);
    RUN_TEST(test_short_tap_on_a_hold_button_does_nothing);
    RUN_TEST(test_hold_restarts_after_sliding_back);
    RUN_TEST(test_hold_progress);
    RUN_TEST(test_bounce_is_a_new_press_not_two_fires_of_one);
    return UNITY_END();
}
```

- [ ] **Step 2: Run it to make sure it fails**

Run: `powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1`
Expected: `FAIL test_press` (no header); the other 49 suites pass.

- [ ] **Step 3: Implement**

`src/util/press.h`:

```cpp
#pragma once
#include <cstdint>
// Touch press tracking for the companion's buttons. PURE, host-tested.
// The button is chosen where the finger goes down. A normal button fires on release if the
// finger is still on it; a hold button fires once, HOLD_MS after the finger is (back) on it.
// Sliding onto another button never fires that one.
// Spec: docs/superpowers/specs/2026-10-14-avatar-home-design.md ("Touch and navigation").
namespace press {
constexpr uint32_t HOLD_MS = 1000;
class Press {
public:
    // down: a finger is on the screen; under: the button id under it (0 = none; ignored when
    // !down); hold: whether `under` is a hold button (read at touch-down). Returns the button
    // that fires now, or 0.
    int update(bool down, int under, bool hold, uint32_t nowMs);
    // 0..1 while a hold button is held on, for its fill; 0 otherwise.
    float holdProgress(uint32_t nowMs) const;
    // The button showing pressed (chosen at touch-down, finger still down), or 0.
    int pressed() const { return down_ ? btn_ : 0; }

private:
    bool down_ = false, fired_ = false, hold_ = false;
    int btn_ = 0;        // chosen at touch-down
    int last_ = 0;       // under the finger at the latest sample
    uint32_t t0_ = 0;    // when the finger was last put (back) on btn_
};
}
```

`src/util/press.cpp`:

```cpp
#include "press.h"

namespace press {
int Press::update(bool down, int under, bool hold, uint32_t nowMs) {
    if (down && !down_) {                    // touch-down: this press belongs to `under`
        down_ = true;
        fired_ = false;
        btn_ = under;
        hold_ = hold;
        last_ = under;
        t0_ = nowMs;
        return 0;
    }
    if (down) {
        if (under == btn_ && last_ != btn_) t0_ = nowMs;   // back on the button: a new full second
        last_ = under;
        if (btn_ && hold_ && !fired_ && under == btn_ && nowMs - t0_ >= HOLD_MS) {
            fired_ = true;
            return btn_;
        }
        return 0;
    }
    if (!down_) return 0;
    down_ = false;                           // release
    const int b = btn_;
    btn_ = 0;
    return (b && !hold_ && last_ == b) ? b : 0;
}

float Press::holdProgress(uint32_t nowMs) const {
    if (!down_ || !hold_ || !btn_ || last_ != btn_) return 0.0f;
    const float p = (float)(nowMs - t0_) / (float)HOLD_MS;
    return p > 1.0f ? 1.0f : p;
}
}
```

- [ ] **Step 4: Run the tests**

Run: `powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1`
Expected: `suites pass=50 fail=0`

- [ ] **Step 5: Commit**

```bash
git add src/util/press.h src/util/press.cpp test/test_press/test_press.cpp
git commit -m "feat(companion): touch press tracking (tap on release, 1 s hold)"
```

---

### Task 2: `util/menunav`: screens, buttons, navigation

**Files:**
- Create: `src/util/menunav.h`, `src/util/menunav.cpp`
- Test: `test/test_menunav/test_menunav.cpp`

**Interfaces:**
- Produces:
  - `menunav::Screen { Home, Lyrics, Sounds, Trip }`;
  - `menunav::Button` (int-backed: `None = 0, Lyrics, Sounds, Trip, Back, Howl, Vroom, Reset`);
  - `struct Rect { int16_t x, y, w, h; }`, `struct Btn { Button id; Rect r; const char* label; }`, `MAX_BTNS` (4), `BAR`;
  - `int buttons(Screen, bool vroom, Btn out[MAX_BTNS])`;
  - `Button hit(Screen, bool vroom, int x, int y)`;
  - `bool isHold(Button)`;
  - `Screen next(Screen, Button)`;
  - `const char* idleText(Screen, bool deckOk, bool wrongKey)`.

- [ ] **Step 1: Write the failing test**

`test/test_menunav/test_menunav.cpp`:

```cpp
#include <unity.h>
#include <cstring>
#include "../../src/util/menunav.h"

using namespace menunav;
void setUp() {}
void tearDown() {}

static const Screen ALL[] = {Screen::Home, Screen::Lyrics, Screen::Sounds, Screen::Trip};

void test_buttons_fit_above_the_bar_and_do_not_overlap() {
    for (Screen s : ALL) {
        for (int v = 0; v < 2; ++v) {
            Btn b[MAX_BTNS];
            const int n = buttons(s, v, b);
            TEST_ASSERT_TRUE(n >= 1 && n <= MAX_BTNS);
            for (int i = 0; i < n; ++i) {
                TEST_ASSERT_TRUE(b[i].r.x >= 0 && b[i].r.y >= 0);
                TEST_ASSERT_TRUE(b[i].r.x + b[i].r.w <= 320);
                TEST_ASSERT_TRUE(b[i].r.y + b[i].r.h <= BAR.y);
                for (int j = 0; j < i; ++j) {
                    const bool apart = b[i].r.x >= b[j].r.x + b[j].r.w || b[j].r.x >= b[i].r.x + b[i].r.w ||
                                       b[i].r.y >= b[j].r.y + b[j].r.h || b[j].r.y >= b[i].r.y + b[i].r.h;
                    TEST_ASSERT_TRUE(apart);
                }
            }
        }
    }
}
void test_home_has_the_three_menu_buttons() {
    Btn b[MAX_BTNS];
    TEST_ASSERT_EQUAL_INT(3, buttons(Screen::Home, false, b));
    TEST_ASSERT_EQUAL_INT(Lyrics, b[0].id);
    TEST_ASSERT_EQUAL_INT(Sounds, b[1].id);
    TEST_ASSERT_EQUAL_INT(Trip, b[2].id);
    TEST_ASSERT_EQUAL_STRING("LYRICS", b[0].label);
}
void test_hit_edges() {
    Btn b[MAX_BTNS];
    buttons(Screen::Home, false, b);
    const Rect r = b[1].r;                                 // SOUNDS
    TEST_ASSERT_EQUAL_INT(Sounds, hit(Screen::Home, false, r.x, r.y));
    TEST_ASSERT_EQUAL_INT(Sounds, hit(Screen::Home, false, r.x + r.w - 1, r.y + r.h - 1));
    TEST_ASSERT_EQUAL_INT(None, hit(Screen::Home, false, r.x + r.w, r.y));
    TEST_ASSERT_EQUAL_INT(None, hit(Screen::Home, false, 50, 50));   // her portrait
}
// Review Focus 4
void test_vroom_only_when_offered() {
    Btn b[MAX_BTNS];
    TEST_ASSERT_EQUAL_INT(2, buttons(Screen::Sounds, false, b));     // HOWL, BACK
    TEST_ASSERT_EQUAL_INT(3, buttons(Screen::Sounds, true, b));      // HOWL, VROOM, BACK
    TEST_ASSERT_EQUAL_INT(Vroom, b[1].id);
    const Rect r = b[1].r;
    TEST_ASSERT_EQUAL_INT(Vroom, hit(Screen::Sounds, true, r.x + 5, r.y + 5));
    TEST_ASSERT_EQUAL_INT(None, hit(Screen::Sounds, false, r.x + 5, r.y + 5));
}
void test_navigation() {
    TEST_ASSERT_TRUE(next(Screen::Home, Lyrics) == Screen::Lyrics);
    TEST_ASSERT_TRUE(next(Screen::Home, Sounds) == Screen::Sounds);
    TEST_ASSERT_TRUE(next(Screen::Home, Trip) == Screen::Trip);
    for (Screen s : ALL) TEST_ASSERT_TRUE(next(s, Back) == Screen::Home);
    TEST_ASSERT_TRUE(next(Screen::Sounds, Howl) == Screen::Sounds);   // actions stay put
    TEST_ASSERT_TRUE(next(Screen::Trip, Reset) == Screen::Trip);
}
void test_every_screen_but_home_has_back() {
    for (Screen s : ALL) {
        if (s == Screen::Home) continue;
        Btn b[MAX_BTNS];
        const int n = buttons(s, true, b);
        bool back = false;
        for (int i = 0; i < n; ++i) back = back || b[i].id == Back;
        TEST_ASSERT_TRUE(back);
    }
}
void test_only_reset_is_a_hold_button() {
    TEST_ASSERT_TRUE(isHold(Reset));
    TEST_ASSERT_FALSE(isHold(Howl));
    TEST_ASSERT_FALSE(isHold(Back));
    TEST_ASSERT_FALSE(isHold(None));
}
void test_idle_text() {
    TEST_ASSERT_EQUAL_STRING("What shall we do, darling?", idleText(Screen::Home, true, false));
    TEST_ASSERT_EQUAL_STRING("", idleText(Screen::Lyrics, true, false));
    TEST_ASSERT_EQUAL_STRING("Pick a sound, darling!", idleText(Screen::Sounds, true, false));
    TEST_ASSERT_EQUAL_STRING("Hold RESET to start a new trip.", idleText(Screen::Trip, true, false));
    TEST_ASSERT_EQUAL_STRING("Looking for the deck...", idleText(Screen::Home, false, false));
    TEST_ASSERT_EQUAL_STRING("Wrong key: REMOTE_TOKEN must match", idleText(Screen::Trip, false, true));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_buttons_fit_above_the_bar_and_do_not_overlap);
    RUN_TEST(test_home_has_the_three_menu_buttons);
    RUN_TEST(test_hit_edges);
    RUN_TEST(test_vroom_only_when_offered);
    RUN_TEST(test_navigation);
    RUN_TEST(test_every_screen_but_home_has_back);
    RUN_TEST(test_only_reset_is_a_hold_button);
    RUN_TEST(test_idle_text);
    return UNITY_END();
}
```

- [ ] **Step 2: Run it to make sure it fails**

Run: `powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1`
Expected: `FAIL test_menunav`; 50 others pass.

- [ ] **Step 3: Implement**

`src/util/menunav.h`:

```cpp
#pragma once
#include <cstdint>
// The companion's screens with the avatar as home: each screen's buttons, the hit test, where a
// button leads and the message bar's idle text. PURE, host-tested.
// Spec: docs/superpowers/specs/2026-10-14-avatar-home-design.md ("Screens").
namespace menunav {
enum class Screen : uint8_t { Home, Lyrics, Sounds, Trip };
enum Button : int { None = 0, Lyrics, Sounds, Trip, Back, Howl, Vroom, Reset };
struct Rect { int16_t x, y, w, h; };
struct Btn {
    Button id;
    Rect r;
    const char* label;
};
constexpr int MAX_BTNS = 4;
constexpr Rect BAR{4, 206, 312, 34};   // the message bar (dialogue box) on every screen
// The buttons on screen s; vroom: the new deck offers that clip. Returns how many.
int buttons(Screen s, bool vroom, Btn out[MAX_BTNS]);
// The button at (x, y) on screen s, or None.
Button hit(Screen s, bool vroom, int x, int y);
// RESET fires after a 1 s hold; every other button on release.
bool isHold(Button b);
// Where button b on screen s leads; action buttons stay on s.
Screen next(Screen s, Button b);
// The message bar when she is quiet: the link state first, then the screen's own hint.
const char* idleText(Screen s, bool deckOk, bool wrongKey);
}
```

`src/util/menunav.cpp`:

```cpp
#include "menunav.h"

namespace menunav {
int buttons(Screen s, bool vroom, Btn out[MAX_BTNS]) {
    int n = 0;
    switch (s) {
    case Screen::Home:                         // the command box at x 200..312
        out[n++] = {Lyrics, {206, 14, 100, 48}, "LYRICS"};
        out[n++] = {Sounds, {206, 66, 100, 48}, "SOUNDS"};
        out[n++] = {Trip, {206, 118, 100, 48}, "TRIP"};
        break;
    case Screen::Lyrics:                       // under her 1x portrait and name tag
        out[n++] = {Back, {4, 110, 92, 36}, "BACK"};
        break;
    case Screen::Sounds:
        out[n++] = {Howl, {20, 16, 280, 64}, "HOWL"};
        if (vroom) out[n++] = {Vroom, {20, 90, 280, 64}, "VROOM"};
        out[n++] = {Back, {20, 164, 100, 36}, "BACK"};
        break;
    case Screen::Trip:
        out[n++] = {Reset, {20, 130, 170, 60}, "RESET (hold)"};
        out[n++] = {Back, {200, 130, 100, 60}, "BACK"};
        break;
    }
    return n;
}

Button hit(Screen s, bool vroom, int x, int y) {
    Btn b[MAX_BTNS];
    const int n = buttons(s, vroom, b);
    for (int i = 0; i < n; ++i) {
        const Rect& r = b[i].r;
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) return b[i].id;
    }
    return None;
}

bool isHold(Button b) { return b == Reset; }

Screen next(Screen s, Button b) {
    switch (b) {
    case Lyrics: return s == Screen::Home ? Screen::Lyrics : s;
    case Sounds: return s == Screen::Home ? Screen::Sounds : s;
    case Trip: return s == Screen::Home ? Screen::Trip : s;
    case Back: return Screen::Home;
    default: return s;
    }
}

const char* idleText(Screen s, bool deckOk, bool wrongKey) {
    if (wrongKey) return "Wrong key: REMOTE_TOKEN must match";
    if (!deckOk) return "Looking for the deck...";
    switch (s) {
    case Screen::Home: return "What shall we do, darling?";
    case Screen::Sounds: return "Pick a sound, darling!";
    case Screen::Trip: return "Hold RESET to start a new trip.";
    default: return "";
    }
}
}
```

- [ ] **Step 4: Run the tests**

Run: `powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1`
Expected: `suites pass=51 fail=0`

- [ ] **Step 5: Commit**

```bash
git add src/util/menunav.h src/util/menunav.cpp test/test_menunav/test_menunav.cpp
git commit -m "feat(companion): menu screens, buttons and navigation"
```

---

### Task 3: Reaction lines in `util/avatarlines`

**Files:**
- Modify: `src/util/avatarlines.h`, `src/util/avatarlines.cpp`, `test/test_avatarlines/test_avatarlines.cpp`

**Interfaces:**
- Produces: `avatarlines::Event` gains `Howl, Vroom, TripReset, Busy, Unreachable`, after `Boot, Track` and in that order. Also `REACT_N` (5) and `REACT[REACT_N]`.

- [ ] **Step 1: Write the failing test**

Add before `int main()` in `test/test_avatarlines/test_avatarlines.cpp`:

```cpp
// Reactions always speak: no gap, no odds.
void test_reactions_always_speak() {
    const Event evs[] = {Event::Howl, Event::Vroom, Event::TripReset, Event::Busy, Event::Unreachable};
    for (int i = 0; i < 5; ++i) {
        avatarlines::In in{evs[i], true, 0, 1, "", "", ""};   // just spoke; rnd fails the odds
        TEST_ASSERT_TRUE(avatarlines::pick(in, out));
        TEST_ASSERT_EQUAL_STRING(avatarlines::REACT[i].text, out.text);
        TEST_ASSERT_EQUAL_UINT8(avatarlines::REACT[i].mood, out.mood);
    }
    TEST_ASSERT_EQUAL_STRING("I can't reach the deck, darling.", avatarlines::REACT[4].text);
    TEST_ASSERT_EQUAL_STRING("One at a time, darling!", avatarlines::REACT[3].text);
}
```

and `RUN_TEST(test_reactions_always_speak);` in `main`.

- [ ] **Step 2: Run it to make sure it fails**

Run: `powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1`
Expected: `FAIL test_avatarlines` (compile error: no `Event::Howl` / `REACT`).

- [ ] **Step 3: Implement**

In `src/util/avatarlines.h`:
- replace `enum class Event : uint8_t { Boot, Track };` with:

  ```cpp
  // Boot / Track follow the gap and odds rules; the reactions (Howl ... Unreachable) always speak.
  enum class Event : uint8_t { Boot, Track, Howl, Vroom, TripReset, Busy, Unreachable };
  ```

- replace `constexpr int BOOT_N = 3, TRACK_N = 12;` with `constexpr int BOOT_N = 3, TRACK_N = 12, REACT_N = 5;`;
- after `extern const Line TRACK[TRACK_N];`, add:

  ```cpp
  extern const Line REACT[REACT_N];   // Howl, Vroom, TripReset, Busy, Unreachable
  ```

In `src/util/avatarlines.cpp`, after the `TRACK` table, add:

```cpp
const Line REACT[REACT_N] = {
    {Happy, "Awoo~! The whole street heard that."},
    {Happy, "Vroom vroom~ Faster, darling!"},
    {Talk, "New trip! Where to, darling?"},
    {Smug, "One at a time, darling!"},
    {Smug, "I can't reach the deck, darling."},
};
```

At the top of `pick`, add before the existing `const Line* l;` logic:

```cpp
    if (in.event >= Event::Howl) {                         // reactions: always, fixed line
        const Line& r = REACT[(int)in.event - (int)Event::Howl];
        out.mood = r.mood;
        fill(r.text, in.title, in.artist, in.poke, out.text, sizeof out.text);
        return true;
    }
```

- [ ] **Step 4: Run the tests**

Run: `powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1`
Expected: `suites pass=51 fail=0` (`test_avatarlines` with 12 tests).

- [ ] **Step 5: Commit**

```bash
git add src/util/avatarlines.h src/util/avatarlines.cpp test/test_avatarlines/test_avatarlines.cpp
git commit -m "feat(avatar): reaction lines for sounds, trip reset, busy and unreachable"
```

---

### Task 4: Touch: `util/touchmap` and `companion/touch`

**Files:**
- Create: `src/util/touchmap.h`, `test/test_touchmap/test_touchmap.cpp`, `src/companion/touch.h`, `src/companion/touch.cpp`
- Modify: `include/pins.h`, `platformio.ini` (`[env:cyd_companion]` `lib_deps`)

**Interfaces:**
- Produces:
  - `touchmap::Cal {x0, x1, y0, y1}`;
  - `touchmap::toScreen(const Cal&, int rawX, int rawY, int* x, int* y)`;
  - `touch::begin()`;
  - `bool touch::read(int* x, int* y)`.

- [ ] **Step 1: Write the failing test**

`test/test_touchmap/test_touchmap.cpp`:

```cpp
#include <unity.h>
#include "../../src/util/touchmap.h"

void setUp() {}
void tearDown() {}

static const touchmap::Cal C{200, 3900, 200, 3900};

void test_corners_and_centre() {
    int x, y;
    touchmap::toScreen(C, 200, 200, &x, &y);
    TEST_ASSERT_EQUAL_INT(0, x);
    TEST_ASSERT_EQUAL_INT(0, y);
    touchmap::toScreen(C, 3900, 3900, &x, &y);
    TEST_ASSERT_EQUAL_INT(319, x);
    TEST_ASSERT_EQUAL_INT(239, y);
    touchmap::toScreen(C, 2050, 2050, &x, &y);
    TEST_ASSERT_EQUAL_INT(159, x);
    TEST_ASSERT_EQUAL_INT(119, y);
}
void test_outside_the_calibration_is_clamped() {
    int x, y;
    touchmap::toScreen(C, 50, 4095, &x, &y);
    TEST_ASSERT_EQUAL_INT(0, x);
    TEST_ASSERT_EQUAL_INT(239, y);
}
void test_reversed_calibration_flips_the_axis() {
    const touchmap::Cal flip{3900, 200, 200, 3900};      // raw x falls left to right
    int x, y;
    touchmap::toScreen(flip, 3900, 200, &x, &y);
    TEST_ASSERT_EQUAL_INT(0, x);
    touchmap::toScreen(flip, 200, 200, &x, &y);
    TEST_ASSERT_EQUAL_INT(319, x);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_corners_and_centre);
    RUN_TEST(test_outside_the_calibration_is_clamped);
    RUN_TEST(test_reversed_calibration_flips_the_axis);
    return UNITY_END();
}
```

- [ ] **Step 2: Run it to make sure it fails**

Run: `powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1`
Expected: `FAIL test_touchmap`; 51 others pass.

- [ ] **Step 3: Implement `touchmap`**

`src/util/touchmap.h`:

```cpp
#pragma once
// XPT2046 raw touch reading -> screen coordinates (320x240, rotation 1). PURE, host-tested.
// A calibration with x0 > x1 (or y0 > y1) flips that axis.
namespace touchmap {
struct Cal { int x0, x1, y0, y1; };   // raw values at screen x 0 / 319 and y 0 / 239

inline int scale(int raw, int r0, int r1, int span) {
    const long v = (long)(raw - r0) * (span - 1) / (r1 - r0);
    return v < 0 ? 0 : v > span - 1 ? span - 1 : (int)v;
}

inline void toScreen(const Cal& c, int rawX, int rawY, int* x, int* y) {
    *x = scale(rawX, c.x0, c.x1, 320);
    *y = scale(rawY, c.y0, c.y1, 240);
}
}
```

Run: `powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1`
Expected: `suites pass=52 fail=0`

- [ ] **Step 4: The driver**

Append to `include/pins.h`, after the touch pins:

```cpp
// Touch calibration: raw XPT2046 readings at the screen's left/right (x) and top/bottom (y)
// edges in rotation 1. Tune from the companion's "[touch] raw x=.. y=.." log lines; swap a pair
// to flip an axis.
static const int TOUCH_RAW_X0 = 200, TOUCH_RAW_X1 = 3900;
static const int TOUCH_RAW_Y0 = 200, TOUCH_RAW_Y1 = 3900;
```

`src/companion/touch.h`:

```cpp
#pragma once
// The CYD's resistive touchscreen (XPT2046) for the companion's buttons. It sits on its own pins
// (include/pins.h); the companion drives it on VSPI because the display owns HSPI.
namespace touch {
void begin();
// True while pressed hard enough; *x, *y in screen coordinates. Logs the raw reading at each
// touch-down ("[touch] raw x=.. y=.. -> x,y") for calibration.
bool read(int* x, int* y);
}
```

`src/companion/touch.cpp`:

```cpp
#include "touch.h"
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include "pins.h"
#include "../util/touchmap.h"

namespace touch {
static constexpr int MIN_Z = 400;                 // lighter readings are noise
static SPIClass s_spi(VSPI);                      // the TFT owns HSPI; the SD slot is unused here
static XPT2046_Touchscreen s_ts(TOUCH_CS, TOUCH_IRQ);
static const touchmap::Cal CAL{TOUCH_RAW_X0, TOUCH_RAW_X1, TOUCH_RAW_Y0, TOUCH_RAW_Y1};
static bool s_wasDown = false;

void begin() {
    s_spi.begin(TOUCH_SCLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    s_ts.begin(s_spi);
    s_ts.setRotation(1);
}

bool read(int* x, int* y) {
    if (!s_ts.touched()) {
        s_wasDown = false;
        return false;
    }
    const TS_Point p = s_ts.getPoint();
    if (p.z < MIN_Z) {
        s_wasDown = false;
        return false;
    }
    touchmap::toScreen(CAL, p.x, p.y, x, y);
    if (!s_wasDown) Serial.printf("[touch] raw x=%d y=%d -> %d,%d\n", p.x, p.y, *x, *y);
    s_wasDown = true;
    return true;
}
}
```

In `platformio.ini`, `[env:cyd_companion]`, add after the `board_build.embed_files` block:

```
lib_deps =
    ${common.lib_deps}
    https://github.com/PaulStoffregen/XPT2046_Touchscreen.git#f956c5d
```

- [ ] **Step 5: Build**

Run: `pio run -e cyd_companion -e s3 -e cyd`
Expected: three SUCCESS. Nothing calls `touch::` yet, so the companion size is unchanged; `s3` and `cyd` are at their pre-branch sizes.

- [ ] **Step 6: Commit**

```bash
git add src/util/touchmap.h test/test_touchmap/test_touchmap.cpp src/companion/touch.h src/companion/touch.cpp include/pins.h platformio.ini
git commit -m "feat(companion): touchscreen driver and calibration mapping"
```

---

### Task 5: Actions to the new deck (`companion/link`)

**Files:**
- Modify: `src/companion/link.h`, `src/companion/link.cpp`

**Interfaces:**
- Produces:
  - `decklink::Action { None, Howl, Vroom, TripReset }`;
  - `decklink::Result { Idle, Pending, Ok, Busy, Failed }`;
  - `void request(Action)`;
  - `Result takeResult(Action* which)`: returns Ok, Busy or Failed once, Pending while waiting, Idle otherwise.

This is device-only code. Its check is the build here and the HOWL press in Task 8.

- [ ] **Step 1: The API**

In `src/companion/link.h`, before `void start();`, add:

```cpp
// Buttons on the companion ask the new deck for something (POST, same REMOTE_TOKEN).
enum class Action : uint8_t { None, Howl, Vroom, TripReset };
// Ok: 2xx. Busy: the deck refused for now (409 busy, 429 too soon, 503 low memory).
// Failed: no answer or any other code.
enum class Result : uint8_t { Idle, Pending, Ok, Busy, Failed };
void request(Action a);              // UI loop; replaces one still waiting
// Ok / Busy / Failed are reported once (with the action), then Idle; Pending while waiting.
Result takeResult(Action* which);
```

- [ ] **Step 2: The POST in the link task**

In `src/companion/link.cpp`, after `static char s_body[1536];`, add:

```cpp
static Action s_want = Action::None;          // requested, not yet sent
static Action s_doneAct = Action::None;       // the action the result belongs to
static Result s_result = Result::Idle;

void request(Action a) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_want = a;
    s_doneAct = a;
    s_result = Result::Pending;
    xSemaphoreGive(s_lock);
}

Result takeResult(Action* which) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const Result r = s_result;
    if (which) *which = s_doneAct;
    if (r == Result::Ok || r == Result::Busy || r == Result::Failed) s_result = Result::Idle;
    xSemaphoreGive(s_lock);
    return r;
}

// POST the action; returns the HTTP status (negative: no answer). 2 s timeouts.
static int postAction(Action a) {
    if (!netcfg::linkUp()) return -1;
    WiFiClient c;
    HTTPClient h;
    h.setTimeout(2000);
    h.setConnectTimeout(2000);
    char url[128];
    if (a == Action::TripReset)
        snprintf(url, sizeof url, "http://%s/trip/reset?k=%s", COMPANION_DECK, REMOTE_TOKEN);
    else
        snprintf(url, sizeof url, "http://%s/play?clip=%s&k=%s", COMPANION_DECK,
                 a == Action::Howl ? "howl" : "vroom", REMOTE_TOKEN);
    if (!h.begin(c, url)) return -1;
    const int rc = h.POST((uint8_t*)nullptr, 0);
    h.end();
    return rc;
}
```

In `run`, at the top of the `for (;;)` body (right after `net::loop();`), add:

```cpp
        xSemaphoreTake(s_lock, portMAX_DELAY);
        const Action a = s_want;
        s_want = Action::None;
        xSemaphoreGive(s_lock);
        if (a != Action::None) {
            const int rc = postAction(a);
            const Result r = (rc >= 200 && rc < 300) ? Result::Ok
                           : (rc == 409 || rc == 429 || rc == 503) ? Result::Busy : Result::Failed;
            Serial.printf("[companion] action %d -> %d\n", (int)a, rc);
            xSemaphoreTake(s_lock, portMAX_DELAY);
            if (s_want == Action::None && s_doneAct == a) s_result = r;   // not replaced meanwhile
            xSemaphoreGive(s_lock);
        }
```

- [ ] **Step 3: Build**

Run: `pio run -e cyd_companion`
Expected: SUCCESS.

- [ ] **Step 4: Commit**

```bash
git add src/companion/link.h src/companion/link.cpp
git commit -m "feat(companion): send Howl / Vroom / trip reset to the new deck"
```

---

### Task 6: `avatarview`: her at any size, her lines for the bar

**Files:**
- Modify (rewrite): `src/companion/avatarview.h`, `src/companion/avatarview.cpp`

**Interfaces:**
- Consumes: `avatarbin`, `avatarlines` (Task 3 events), `avatarscene`, `avatarsing`, `decklink::View`.
- Produces:
  - `avatarview::begin()`, `on()`, `invalidate()`;
  - `void place(int x, int y, int scale)` (scale 0 hides her);
  - `void say(avatarlines::Event, const decklink::View&, uint32_t nowMs)`;
  - `const char* update(TFT_eSPI&, const decklink::View&, uint32_t nowMs)`, which returns her line typed so far, or nullptr.

This is device-only code. Its checks are the build here and the on-screen check in Task 8.

- [ ] **Step 1: Rewrite the header**

`src/companion/avatarview.h`:

```cpp
#pragma once
#include <TFT_eSPI.h>
#include "link.h"
#include "../util/avatarlines.h"
// The deck avatar on the companion (data/avatar.bin, embedded in this build only): her portrait
// at the spot and size the current screen gives her, singing along with synced lyrics, and her
// lines for the message bar. Spec: docs/superpowers/specs/2026-10-14-avatar-home-design.md
namespace avatarview {
// Parses the embedded file; false (no avatar: the companion's lyrics-only screen) when there is
// none. Logs "[avatar] ...".
bool begin();
bool on();
void invalidate();                         // repaint her (and her name tag) on the next update
// Where the current screen shows her: top-left corner and scale (1 = 92x92, 2 = 184x184,
// 0 = not on this screen). A change repaints her on the next update.
void place(int x, int y, int scale);
// She says the line for this event now (reactions always; Boot / Track by avatarlines' rules).
void say(avatarlines::Event ev, const decklink::View& v, uint32_t nowMs);
// Per frame: the boot greeting and song-change remarks from the feed, her portrait (her line's
// mood, else singing along, else the lollipop), and her line typed so far for the message bar
// (nullptr when she is quiet).
const char* update(TFT_eSPI& t, const decklink::View& v, uint32_t nowMs);
}
```

- [ ] **Step 2: Rewrite the implementation**

`src/companion/avatarview.cpp`:

```cpp
#include "avatarview.h"
#include <Arduino.h>
#include <cstring>
#include <esp_system.h>
#include "../util/avatarbin.h"
#include "../util/avatarscene.h"
#include "../util/avatarsing.h"

extern const uint8_t _binary_data_avatar_bin_start[] asm("_binary_data_avatar_bin_start");
extern const uint8_t _binary_data_avatar_bin_end[] asm("_binary_data_avatar_bin_end");

namespace avatarview {
static constexpr uint16_t TAG_PINK = 0xF497;   // #F090B8
static avatarbin::Info s_info{};
static int s_x = 0, s_y = 0, s_scale = 0;        // where the current screen shows her
static int s_entry = -1, s_frame = -1;           // on screen (-1: repaint)
static bool s_tag = false;
static bool s_talking = false;                    // her own line is up
static uint32_t s_t0 = 0;
static int s_lineEntry = 0;
static char s_text[avatarlines::TEXT_CAP], s_typed[avatarlines::TEXT_CAP];

bool begin() {
    const char* why = "";
    const size_t len = (size_t)(_binary_data_avatar_bin_end - _binary_data_avatar_bin_start);
    if (avatarbin::parse(_binary_data_avatar_bin_start, len, s_info, &why)) {
        Serial.printf("[avatar] %s: %d moods\n", s_info.name, s_info.moods);
        return true;
    }
    Serial.printf("[avatar] off: %s\n", why);
    return false;
}

bool on() { return s_info.moods > 0; }
void invalidate() { s_entry = s_frame = -1; s_tag = false; }

void place(int x, int y, int scale) {
    if (x == s_x && y == s_y && scale == s_scale) return;
    s_x = x;
    s_y = y;
    s_scale = scale;
    invalidate();
}

// Frame f of entry e at (s_x, s_y), 1x or pixel-doubled 2x, row by row through a RAM buffer
// (the embedded file need not be 2-byte aligned); then her name tag, once.
static void show(TFT_eSPI& t, int e, int f) {
    constexpr int N = avatarbin::SIDE;
    if (s_scale == 0 || (e == s_entry && f == s_frame)) return;
    const uint8_t* px = _binary_data_avatar_bin_start + avatarbin::frameOffset(s_info, e, f);
    static uint16_t src[N], row[2 * N];
    for (int y = 0; y < N; ++y) {
        memcpy(src, px + (size_t)y * N * 2, N * 2);   // RGB565 high byte first, pushed as stored
        if (s_scale == 1) {
            t.pushImage(s_x, s_y + y, N, 1, src);
            continue;
        }
        for (int x = 0; x < N; ++x) row[2 * x] = row[2 * x + 1] = src[x];
        t.pushImage(s_x, s_y + 2 * y, 2 * N, 1, row);
        t.pushImage(s_x, s_y + 2 * y + 1, 2 * N, 1, row);
    }
    s_entry = e;
    s_frame = f;
    if (!s_tag) {                                 // the name tag, centred under her
        const int side = N * s_scale;
        const int w = (int)strlen(s_info.name) * 6 + 6;
        const int x = s_x + (side - w) / 2;
        t.fillRect(x, s_y + side + 2, w, 9, TAG_PINK);
        t.setTextDatum(TL_DATUM);
        t.setTextColor(TFT_WHITE, TAG_PINK);
        t.drawString(s_info.name, x + 3, s_y + side + 3, 1);
        s_tag = true;
    }
}

void say(avatarlines::Event ev, const decklink::View& v, uint32_t now) {
    static bool spoke = false;
    static uint32_t lastAt = 0;
    static avatarlines::Out out;
    if (!on()) return;
    const avatarlines::In in{ev, spoke, now - lastAt, esp_random(), v.deck.track, v.deck.artist, ""};
    if (!avatarlines::pick(in, out)) return;
    spoke = true;
    lastAt = now;
    s_lineEntry = avatarbin::find(s_info, out.mood);
    strlcpy(s_text, out.text, sizeof s_text);
    s_t0 = now;
    s_talking = true;
    Serial.printf("[avatar] says: %s\n", out.text);
}

const char* update(TFT_eSPI& t, const decklink::View& v, uint32_t now) {
    if (!on()) return nullptr;
    static bool greeted = false;
    static uint32_t gen = 0;
    if (v.polledAtMs && v.deckOk) {              // events come from the new deck's feed
        if (!greeted) {
            greeted = true;
            gen = v.deck.trackGen;
            say(avatarlines::Event::Boot, v, now);
        } else if (v.deck.trackGen != gen) {
            gen = v.deck.trackGen;
            say(avatarlines::Event::Track, v, now);
        }
    }
    if (s_talking) {                             // her line: typing + hold
        const avatarscene::State st = avatarscene::at(now - s_t0 + avatarscene::SLIDE_MS, s_text,
                                                      s_info.frames[s_lineEntry]);
        if (st.phase != avatarscene::Phase::Done) {
            show(t, s_lineEntry, st.frame);
            memcpy(s_typed, s_text, st.bytes);
            s_typed[st.bytes] = '\0';
            return s_typed;
        }
        s_talking = false;
    }
    const stateparse::Deck& d = v.deck;
    const int talk = avatarbin::find(s_info, avatarbin::Talk);
    const bool synced = strcmp(d.lyricKind, "synced") == 0;
    if (v.polledAtMs && d.playing && synced && d.line[0] && s_info.id[talk] == avatarbin::Talk) {
        const uint32_t pos = d.progressMs + (now - v.polledAtMs);
        uint32_t start = d.lineStartMs, next = d.nextStartMs;
        if (next && pos >= next) {               // the next line is already on screen
            start = next;
            next = 0;
        }
        show(t, talk, avatarsing::frame(pos, start, next, s_info.frames[talk]));
    } else {
        show(t, avatarbin::find(s_info, avatarbin::Smug), 0);
    }
    return nullptr;
}
}
```

- [ ] **Step 3: Build**

`dashboard.cpp` still calls the old `update` and uses `COL_X`. Task 7 rewrites it. To keep this task building on its own, make these two changes in `src/companion/dashboard.cpp`:
- change `avatarview::COL_X` to `104`;
- change `const char* say = avatarview::update(t, v, nowMs);` to `const char* say = nullptr;`.

Run: `pio run -e cyd_companion`
Expected: SUCCESS.

- [ ] **Step 4: Commit**

```bash
git add src/companion/avatarview.h src/companion/avatarview.cpp src/companion/dashboard.cpp
git commit -m "feat(avatar): draw her at 1x or 2x anywhere; her lines for the message bar"
```

---

### Task 7: The screens and the app (`companion/screens`, `dashboard`, `app`)

**Files:**
- Create: `src/companion/screens.h`, `src/companion/screens.cpp`
- Modify: `src/companion/dashboard.h`, `src/companion/dashboard.cpp`, `src/companion/app.cpp`

**Interfaces:**
- Consumes: Tasks 1–6 and `ui::battleBox`, `ui::dialogueBox`, `ui::drawText`, `theme::pal()`.
- Produces:
  - `screens::draw(TFT_eSPI&, menunav::Screen, bool vroom, menunav::Button cursor)`;
  - `screens::button(TFT_eSPI&, const menunav::Btn&, bool pressed, float fill, bool cursor)`;
  - `screens::bar(TFT_eSPI&, const char*)`;
  - `screens::tripTime(TFT_eSPI&, uint32_t seconds)`;
  - `screens::invalidate()`;
  - `dashboard::draw(...)` (now the lyric column only when the avatar is on).

This is device-only code. Its checks are the build here and Task 8 on the old deck.

- [ ] **Step 1: `screens`**

`src/companion/screens.h`:

```cpp
#pragma once
#include <TFT_eSPI.h>
#include "../util/menunav.h"
// Drawing for the companion's avatar-home screens: background, buttons, the message bar and the
// trip time. Spec: docs/superpowers/specs/2026-10-14-avatar-home-design.md ("Screens").
namespace screens {
// Repaints screen s: background, its buttons (cursor marks the last menu choice on Home), and
// an empty message bar. Her portrait and the lyric column are drawn by their own modules.
void draw(TFT_eSPI& t, menunav::Screen s, bool vroom, menunav::Button cursor);
// One button: normal, pressed (dark), with a hold fill 0..1 from the left; cursor: the "▶" mark.
void button(TFT_eSPI& t, const menunav::Btn& b, bool pressed, float fill, bool cursor);
// The message bar's text, repainted only when it changes.
void bar(TFT_eSPI& t, const char* text);
// The trip time (h:mm, or m:ss under an hour), repainted when the shown text changes.
void tripTime(TFT_eSPI& t, uint32_t seconds);
void invalidate();   // forget the shown bar text and trip time (after a full repaint)
}
```

`src/companion/screens.cpp`:

```cpp
#include "screens.h"
#include <cstdio>
#include <cstring>
#include "../ui/battle.h"
#include "../ui/textdraw.h"
#include "../ui/theme.h"

namespace screens {
static char s_bar[160] = "\x01", s_trip[16] = "\x01";

void invalidate() {
    s_bar[0] = '\x01';
    s_trip[0] = '\x01';
}

void button(TFT_eSPI& t, const menunav::Btn& b, bool pressed, float fill, bool cursor) {
    const menunav::Rect& r = b.r;
    const uint16_t face = pressed ? theme::pal().dlgFrame : theme::pal().boxFill;
    t.fillRoundRect(r.x, r.y, r.w, r.h, 6, face);
    if (fill > 0.0f) t.fillRoundRect(r.x, r.y, (int)(r.w * fill), r.h, 6, theme::pal().dlgFrame);
    t.drawRoundRect(r.x, r.y, r.w, r.h, 6, theme::pal().boxBorder);
    const uint16_t fg = (pressed || fill > 0.0f) ? TFT_WHITE : theme::pal().text;
    ui::drawText(t, b.label, r.x + r.w / 2, r.y + r.h / 2, fg, theme::pal().textShadow, MC_DATUM, r.w - 16);
    if (cursor) t.fillTriangle(r.x + 6, r.y + r.h / 2 - 6, r.x + 6, r.y + r.h / 2 + 6, r.x + 14,
                               r.y + r.h / 2, theme::pal().text);
}

void draw(TFT_eSPI& t, menunav::Screen s, bool vroom, menunav::Button cursor) {
    t.fillScreen(theme::pal().top);
    if (s == menunav::Screen::Home) ui::battleBox(t, 200, 8, 112, 162, ui::Tab::None);   // command box
    menunav::Btn b[menunav::MAX_BTNS];
    const int n = menunav::buttons(s, vroom, b);
    for (int i = 0; i < n; ++i)
        button(t, b[i], false, 0.0f, s == menunav::Screen::Home && b[i].id == cursor);
    invalidate();
    bar(t, "");
}

void bar(TFT_eSPI& t, const char* text) {
    const char* s = text ? text : "";
    if (strcmp(s, s_bar) == 0) return;
    strlcpy(s_bar, s, sizeof s_bar);
    const menunav::Rect& r = menunav::BAR;
    ui::dialogueBox(t, r.x, r.y, r.w, r.h);
    if (s[0]) ui::drawText(t, s, r.x + 12, r.y + r.h / 2, theme::pal().text, theme::pal().dlgShadow,
                           ML_DATUM, r.w - 24);
}

void tripTime(TFT_eSPI& t, uint32_t seconds) {
    char txt[16];
    if (seconds >= 3600) snprintf(txt, sizeof txt, "%lu:%02lu", (unsigned long)(seconds / 3600),
                                  (unsigned long)(seconds / 60 % 60));
    else snprintf(txt, sizeof txt, "%lu:%02lu", (unsigned long)(seconds / 60), (unsigned long)(seconds % 60));
    if (strcmp(txt, s_trip) == 0) return;
    strlcpy(s_trip, txt, sizeof s_trip);
    t.fillRect(20, 30, 280, 90, theme::pal().top);
    t.setTextDatum(MC_DATUM);
    t.setTextColor(theme::pal().topText, theme::pal().top);
    t.setTextSize(2);
    t.drawString(txt, 160, 75, 4);
    t.setTextSize(1);
}
}
```

- [ ] **Step 2: `dashboard` becomes the Lyrics content**

In `src/companion/dashboard.cpp`:
- In `lyrics()`, change:
  - `const int x0 = avatarview::on() ? 104 : 0;` (keep the comment);
  - `t.fillRect(x0, 0, W - x0, H - 22, theme::pal().top);` to `t.fillRect(x0, 0, W - x0, avatarview::on() ? menunav::BAR.y - 2 : H - 22, theme::pal().top);`.
- Add `#include "../util/menunav.h"`.
- Replace `draw` with:

  ```cpp
  void draw(TFT_eSPI& t, const decklink::View& v, uint32_t nowMs) {
      const bool full = !avatarview::on();     // no avatar: today's screen, with its status strip
      if (v.wrongKey) {
          lyrics(t, "Wrong key", "REMOTE_TOKEN must match on both decks");
          if (full) status(t, "");
      } else if (!v.polledAtMs) {
          lyrics(t, "Looking for the deck...", "Is the new deck on, on the same hotspot?");
          if (full) status(t, "");
      } else {
          const lyricclock::View lv = lyricclock::pick(v.deck, nowMs - v.polledAtMs);
          lyrics(t, lv.big, lv.small);
          if (full) status(t, v.deckOk ? "" : "Looking for the deck...");
      }
  }
  ```

The block that called `avatarview::update` / `say` goes away; `app` drives her now.

In `src/companion/dashboard.h`, update the comment to: "The Lyrics screen: the current line big (2x when it fits in two rows, else 1x), the next line dim under it. With the avatar it's a column right of her portrait (x 104..316, above the message bar); without one, the whole screen plus a status line. Repainted only when what it shows changes."

- [ ] **Step 3: `app` runs the screens**

Replace `src/companion/app.cpp`:

```cpp
// Companion screen (env cyd_companion): the CYD beside the S3. With an avatar (data/avatar.bin)
// she is the home screen and the menu (LYRICS / SOUNDS / TRIP, touch); without one it shows the
// lyrics big, as before. It reads the S3's GET /state over the hotspot.
// Specs: docs/superpowers/specs/2026-10-13-companion-screen-design.md,
//        docs/superpowers/specs/2026-10-14-avatar-home-design.md
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "../board/board.h"
#include "../net/wifi.h"
#include "../net/clock.h"
#include "../ui/backlight.h"
#include "../ui/theme.h"
#include "../util/daynight.h"
#include "../util/menunav.h"
#include "../util/press.h"
#include "../util/version.h"
#include "avatarview.h"
#include "dashboard.h"
#include "link.h"
#include "screens.h"
#include "touch.h"

static TFT_eSPI s_tft;
static menunav::Screen s_screen = menunav::Screen::Home;
static menunav::Button s_cursor = menunav::Lyrics;   // last menu choice (the ▶ on Home)
static bool s_vroom = false;                          // the new deck offers VROOM (as drawn)
static bool s_waiting = false;                        // an action is on its way to the deck
static press::Press s_press;

// Moves to screen s and repaints it whole.
static void enter(menunav::Screen s) {
    s_screen = s;
    screens::draw(s_tft, s, s_vroom, s_cursor);
    if (s == menunav::Screen::Home) avatarview::place(8, 8, 2);
    else if (s == menunav::Screen::Lyrics) avatarview::place(4, 4, 1);
    else avatarview::place(0, 0, 0);
    avatarview::invalidate();
    dashboard::invalidate();
}

static bool findBtn(menunav::Button id, menunav::Btn* out) {
    menunav::Btn b[menunav::MAX_BTNS];
    const int n = menunav::buttons(s_screen, s_vroom, b);
    for (int i = 0; i < n; ++i)
        if (b[i].id == id) { *out = b[i]; return true; }
    return false;
}

static void redrawButton(menunav::Button id, bool pressed, float fill) {
    menunav::Btn b;
    if (id != menunav::None && findBtn(id, &b))
        screens::button(s_tft, b, pressed, fill,
                        s_screen == menunav::Screen::Home && id == s_cursor);
}

// Touch -> press -> a button fires: navigate, or send an action to the deck.
static void handleTouch(uint32_t now) {
    int x = 0, y = 0;
    const bool down = touch::read(&x, &y);
    const menunav::Button under = down ? menunav::hit(s_screen, s_vroom, x, y) : menunav::None;
    const menunav::Button before = (menunav::Button)s_press.pressed();
    const menunav::Button fired =
        (menunav::Button)s_press.update(down, under, menunav::isHold(under), now);
    const menunav::Button after = (menunav::Button)s_press.pressed();
    if (before != after) {                        // pressed look follows the finger
        redrawButton(before, false, 0.0f);
        redrawButton(after, true, 0.0f);
    }
    static uint32_t lastFill = 0;
    if (after == menunav::Reset && now - lastFill >= 50) {   // RESET fills while held
        lastFill = now;
        redrawButton(after, true, s_press.holdProgress(now));
    }
    if (fired == menunav::None) return;
    switch (fired) {
    case menunav::Howl: decklink::request(decklink::Action::Howl); s_waiting = true; break;
    case menunav::Vroom: decklink::request(decklink::Action::Vroom); s_waiting = true; break;
    case menunav::Reset:
        decklink::request(decklink::Action::TripReset);
        s_waiting = true;
        redrawButton(menunav::Reset, false, 0.0f);
        break;
    default:                                      // navigation
        if (s_screen == menunav::Screen::Home) s_cursor = fired;
        enter(menunav::next(s_screen, fired));
        break;
    }
}

// The deck answered an action: she reacts.
static void handleResult(const decklink::View& v, uint32_t now) {
    decklink::Action a = decklink::Action::None;
    const decklink::Result r = decklink::takeResult(&a);
    if (r == decklink::Result::Pending || r == decklink::Result::Idle) return;
    s_waiting = false;
    avatarlines::Event ev = avatarlines::Event::Unreachable;
    if (r == decklink::Result::Busy) ev = avatarlines::Event::Busy;
    else if (r == decklink::Result::Ok)
        ev = a == decklink::Action::Howl ? avatarlines::Event::Howl
           : a == decklink::Action::Vroom ? avatarlines::Event::Vroom : avatarlines::Event::TripReset;
    avatarview::say(ev, v, now);
}

void setup() {
    Serial.begin(115200);
    board::begin();
    Serial.printf("[boot] cardeck companion v%s\n", FW_VERSION);
    s_tft.init(); s_tft.invertDisplay(true); s_tft.setRotation(1);
    s_tft.fillScreen(theme::pal().top);
    backlight::begin();
    s_tft.setTextColor(theme::pal().topText, theme::pal().top);
    s_tft.drawString("Connecting to WiFi...", 10, 10, 2);
    net::connectAny();          // if it fails, the link task keeps retrying (net::loop)
    netclock::begin();          // night palette
    decklink::start();
    if (avatarview::begin()) {
        touch::begin();
        enter(menunav::Screen::Home);
    } else {
        s_tft.fillScreen(theme::pal().top);
        dashboard::invalidate();
    }
}

void loop() {
    static uint32_t lastNightCheck = 0;
    if (lastNightCheck == 0 || millis() - lastNightCheck > 60000) {
        lastNightCheck = millis() | 1;
        int minute = 0;
        const bool night = netclock::minuteOfDay(&minute) && daynight::isNight(minute);
        if (night != theme::isNightActive()) {
            theme::setNight(night);
            backlight::set(night);
            if (avatarview::on()) enter(s_screen);
            else {
                s_tft.fillScreen(theme::pal().top);
                dashboard::invalidate();
            }
        }
    }
    decklink::View v;
    decklink::snapshot(v);
    const uint32_t now = millis();
    if (!avatarview::on()) {                      // no avatar: lyrics only, touch ignored
        dashboard::draw(s_tft, v, now);
        delay(20);
        return;
    }
    if (v.deck.hasVroom != s_vroom) {             // the deck's clip list changed
        s_vroom = v.deck.hasVroom;
        if (s_screen == menunav::Screen::Sounds) enter(s_screen);
    }
    handleTouch(now);
    handleResult(v, now);
    const char* line = avatarview::update(s_tft, v, now);
    if (s_screen == menunav::Screen::Lyrics) dashboard::draw(s_tft, v, now);
    if (s_screen == menunav::Screen::Trip && v.polledAtMs)       // ticks locally between polls
        screens::tripTime(s_tft, v.deck.tripS + (now - v.polledAtMs) / 1000);
    screens::bar(s_tft, line ? line : s_waiting ? "..." : menunav::idleText(s_screen, v.deckOk, v.wrongKey));
    delay(20);
}
```

- [ ] **Step 4: Build all three**

Run: `pio run -e cyd_companion -e s3 -e cyd`
Expected: three SUCCESS; `cyd_companion` about 52 %; `s3` and `cyd` unchanged.

- [ ] **Step 5: Host tests**

Run: `powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1`
Expected: `suites pass=52 fail=0`

- [ ] **Step 6: Commit**

```bash
git add src/companion/screens.h src/companion/screens.cpp src/companion/dashboard.h src/companion/dashboard.cpp src/companion/app.cpp
git commit -m "feat(companion): the avatar home screen with LYRICS / SOUNDS / TRIP"
```

---

### Task 8: On the old deck, docs

- [ ] **Step 1: Flash**

Run `python tools/gen_avatar.py` if `data/avatar.bin` is missing (expect 67 736 bytes). Then:
- old deck: `pio run -e cyd_companion -t upload --upload-port COM13`;
- new deck, if it isn't already on this branch's unchanged firmware: `pio run -e s3 -t upload --upload-port COM12`.

Capture the old deck's log: `python tools/capture_serial.py COM13 600 home.log`. Both decks need to be on the iPhone hotspot.

- [ ] **Step 2: Calibrate.** Touch the four corners. Read the `[touch] raw x=.. y=..` lines, and if the mapped coordinates are off by more than about 10 px, set `TOUCH_RAW_*` in `include/pins.h` from them. Reflash.

- [ ] **Step 3: Check on the deck**

| Check | Expected |
|---|---|
| Boot | Home: her at 2× with the name tag, the command box, "What shall we do, darling?"; then her boot line types in the bar |
| Tap each menu button | it shows pressed while touched, and opens its screen on release; sliding off cancels |
| BACK on each screen | Home, with `▶` on the last choice |
| Lyrics | her at 1× in the corner, singing along with synced lyrics, the lyric column to her right |
| Sounds → HOWL | the new deck howls; the bar shows "..." then "Awoo~! ..."; a second tap at once gives "One at a time, darling!" (Review Focus 3) |
| Trip → a short tap on RESET | nothing; a 1 s hold fills the button and resets: "New trip! ...", and the time goes to 0:00 after the next poll |
| Unplug the new deck, tap HOWL | "I can't reach the deck, darling."; the bar idles on "Looking for the deck..." |
| Night mode | the current screen repaints in the night palette |
| `[mem]` | steady over 10 min |

- [ ] **Step 4: Docs**

**`README.md` Features:**

```markdown
- **Avatar (old deck):** on the companion screen a character of your choice is the home screen:
  shown big beside a Pokemon-style menu (LYRICS / SOUNDS / TRIP, by touch), she sings along with
  synced lyrics, greets you, remarks on new songs and reacts when you play a sound or reset the
  trip. Her portraits come from your own images (`tools/gen_avatar.py`).
```

**`README.md` Make-it-yours row:**

```markdown
| Avatar (companion) | images + `avatar-photos/avatar.json`, then `python tools/gen_avatar.py` (needs ffmpeg; see the script's header) | lyrics only |
```

**`CHANGELOG.md`, `[Unreleased]` / `Added`:**

```markdown
- Avatar on the companion (old deck): your character is the home screen, drawn 2x beside a
  touch menu (Lyrics, Sounds, Trip). She sings along with synced lyrics, greets you, remarks on
  new songs (at most every 5 min) and reacts to Howl / trip reset. Portraits come from your own
  images via `tools/gen_avatar.py` (`avatar-photos/`, git-ignored); without one the companion
  shows the lyrics as before. The companion now sends Howl and trip reset to the new deck.
```

**`docs/ARCHITECTURE.md`**, a bullet after **Companion screen**:

```markdown
- **Avatar home** (`companion/{avatarview,screens,touch,app}`, `util/{avatarbin,avatarlines,
  avatarscene,avatarsing,menunav,press,touchmap}`) — on the companion only. `data/avatar.bin`
  (from `tools/gen_avatar.py`; `tools/avatar_clip.py` writes an empty one) holds 92×92 RGB565
  frames per mood, drawn 1× or pixel-doubled from flash. The touchscreen (XPT2046, VSPI) feeds
  `press` (tap on release, 1 s hold) and `menunav` (screens, buttons, navigation); actions go to
  the new deck through `companion/link` (POST `/play`, `/trip/reset`). Her lines come from
  `avatarlines` and type out in the message bar (`avatarscene`); she sings along by `avatarsing`.
```

Also add `avatarbin,avatarlines,avatarscene,avatarsing,menunav,press,touchmap` to the host-tested `util/{...}` list.

**`docs/BACKLOG.md`:**
- under **Ideas**:

  ```markdown
  - **Avatar, next** (home screen built 2026-10-14): more menu items (Weather, Map, OBD); the
    Claude assistant (Siri Shortcut + `POST /say`); blinking; VROOM on the new deck.
  ```

- under **Verification (on device)**: any Step 3 row that wasn't checked.

- [ ] **Step 5: Full checks and commit**

Run: `powershell -ExecutionPolicy Bypass -File tools/run_tests.ps1`, `python tools/test_gen_avatar.py`, `pio run -e s3 -e cyd -e cyd_companion`
Expected: `suites pass=52 fail=0`; `OK`; three SUCCESS.

```bash
git add README.md CHANGELOG.md docs/ARCHITECTURE.md docs/BACKLOG.md include/pins.h
git commit -m "docs: avatar home screen on the companion"
```
