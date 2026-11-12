# Terminal prompt line, layout B and link icons: implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Put the yellow deck's prompt line at the bottom of every screen, lay out the main screen as
layout B, and give both decks a GPS state and a deck-link state.

**Architecture:**
- **Shared rules:** one pure module, `util/common/decklink`, holds the link rule (15 s), the
  `/link` body and its parse, and the yellow deck's message tracker. Both decks use it.
- **Geometry:** the rects live in `util/yellow/deckmenu` and the text positions in
  `ui/termlayout`, both host-tested as today.
- **Black deck:** `/link` is answered by `net/remote`.
- **Yellow deck:** its `deck` task asks `/link` every 5 s, and `ledpanel.cpp` draws the tags and
  the shared prompt line.

**Tech Stack:** C++ (Arduino-ESP32, PlatformIO envs `ledpanel` and `s3`), TFT_eSPI, Unity host tests
via `tools/run_tests.ps1`.

**Spec:** `docs/superpowers/specs/2026-11-12-terminal-line-and-link-icons-design.md` (read it first).

## Global Constraints

- **Host tests:** `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1` passes after every task.
- **Builds:** `pio run -e ledpanel -e s3` succeeds at the end. `ledpanel` must still fit its slot
  (86.7 % today); report the new %.
- **Yellow heap:** no new FreeRTOS task, no TLS, no new buffer over 64 bytes on the yellow deck.
  Its lowest free heap online is ~37 KB.
- **Link:** a deck counts the link as up while it has had a valid exchange within `LINK_MS = 15000`
  ms. The yellow deck asks `/link` every `CHECK_MS = 5000` ms, regardless of the other asks.
- **What counts as hearing the yellow deck:** on the black deck, `/link`, `/music` and `/karaoke`
  count. `/playlists/play` does not (the watch sends it too).
- **Messages (exact copy):** `>DECK: LINKED`, `>DECK: NO LINK` (red), `>GPS: FIX`, `>GPS: NO FIX`
  (red).
- **Yellow tags (exact copy):** `GPS`, `DECK`.
- **Flashing:** the yellow deck over USB on COM16. The black deck (COM12) only after asking João,
  each time. Stop any serial capture before flashing; open the S3's serial with dtr/rts False.
- **Commits:** conventional commits, each ending with
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. No secrets; config stays in
  `src/config.h`. No push, merge or PR unless asked.
- **Code comments:** they follow the files' existing voice: plain English, the why, units.

## Review Focus

1. **`millis()` wrap and "never heard" (0)** must not read as linked. This is pinned in Task 1:
   `test_never_heard_is_not_linked` and `test_wrap`.
2. **The first state after boot is silent, and so are repeats.** A deck that boots linked must not
   say `>DECK: LINKED`, and a state that flickers back within the window must not message twice.
   Pinned in Task 1: `test_tracker_*`.
3. **The black deck is unplugged while karaoke is on.** `>BLACK DECK NOT ANSWERING` already shows,
   so `>DECK: NO LINK` must not follow within 15 s. Pinned in Task 1:
   `test_tracker_skips_after_not_answering`.
4. **A tap in the 6 px gap between the lights panel and the switches** must snap to the nearer
   button, and a tap on the prompt line must hit nothing, on every screen. Pinned in Task 2:
   `test_prompt_band_is_no_hit_everywhere`, `test_menu_gap_snaps`.
5. **The longest messages fit the bottom line with the cursor.** That includes `>LAST TRIP 999.9 KM
   99:59` and `>BLACK DECK NOT ANSWERING`. With the pad on, the header tags don't touch the title.
   Pinned in Task 3: `test_prompt_line_fits`, `test_link_tags_fit`.

---

### Task 1: `decklink`, the shared link rules (pure)

**Files:**
- Create: `src/util/common/decklink.h`, `src/util/common/decklink.cpp`
- Test: `test/test_decklink/test_decklink.cpp`

**Interfaces:**
- Produces:
```cpp
namespace decklink {
constexpr uint32_t LINK_MS = 15000, CHECK_MS = 5000;
enum class Gps : uint8_t { Unknown, Fix, NoFix };
// heardAt: millis() of the last valid exchange, 0 = never (callers store millis() | 1). Wrap-safe.
bool linked(uint32_t heardAt, uint32_t now);
void body(bool fix, char* out, size_t n);     // "gps=1" / "gps=0"
Gps parse(const char* body);                 // "gps=1" Fix, "gps=0" NoFix, anything else Unknown
Gps shown(bool isLinked, Gps last);          // Unknown when !isLinked
extern const char* const LINKED_MSG;   // ">DECK: LINKED"
extern const char* const NOLINK_MSG;   // ">DECK: NO LINK"
extern const char* const FIX_MSG;      // ">GPS: FIX"
extern const char* const NOFIX_MSG;    // ">GPS: NO FIX"
bool isAlert(const char* msg);         // true for NOLINK_MSG and NOFIX_MSG
// The yellow deck's messages: one call per loop pass with what the header shows. Returns the
// message to show now (one of the four) or nullptr. The first known state of each is silent; a
// link drop is silent when notAnsweringAt (millis() of the last ">BLACK DECK NOT ANSWERING", 0 =
// never) is under LINK_MS old. GPS messages only while linked (Unknown is no change).
class Tracker {
public:
    const char* update(bool isLinked, Gps g, uint32_t now, uint32_t notAnsweringAt);
private:
    int8_t link_ = -1, gps_ = -1;   // -1 not known yet
};
}
```

- [ ] **Step 1: Write the failing tests.** In `test_decklink.cpp`, Unity style like
  `test/test_deckaddr`:
  - `test_never_heard_is_not_linked`: `linked(0, 5000)` is false.
  - `test_linked_window`: `linked(1000, 15999)` is true and `linked(1000, 16000)` is false.
  - `test_wrap`: `linked(0xFFFFF000u, 0x00000F00u)` is true (0x1F00 ms later).
  - `test_body_and_parse`:
    - `body(true)` gives `"gps=1"` and `body(false)` gives `"gps=0"`;
    - `parse` of each gives Fix / NoFix;
    - `parse("")`, `parse(nullptr)` and `parse("gps=2")` give Unknown;
    - `parse("gps=1\n")` gives Fix (a trailing newline is tolerated).
  - `test_shown`: `shown(false, Gps::Fix) == Gps::Unknown` and `shown(true, Gps::NoFix) == Gps::NoFix`.
  - `test_is_alert`: true for `">DECK: NO LINK"` and `">GPS: NO FIX"`; false for `">DECK: LINKED"`
    and `">GPS: FIX"`.
  - `test_tracker_first_state_silent`: the first `update(true, Fix, …)` gives nullptr.
  - `test_tracker_drop_and_back`:
    - after a silent first update, `update(false, Unknown, …)` gives `NOLINK_MSG`;
    - `update(true, Fix, …)` then gives `LINKED_MSG`;
    - the same again gives nullptr.
  - `test_tracker_gps_change`: linked throughout, Fix → NoFix gives `NOFIX_MSG`, and NoFix → Fix
    gives `FIX_MSG`.
  - `test_tracker_unknown_is_no_change`: linked Fix, then link lost (gives `NOLINK_MSG`), then
    re-linked with NoFix. That update gives `LINKED_MSG`, and the next update (still NoFix) gives
    `NOFIX_MSG`. One message per pass; the link message comes first.
  - `test_tracker_skips_after_not_answering`: linked, then `update(false, Unknown, now=20000,
    notAnsweringAt=10000)` gives nullptr. The state is still recorded, so the later re-link gives
    `LINKED_MSG`.
- [ ] **Step 2: Run them and check they fail.** Run `powershell -ExecutionPolicy Bypass -File
  tools\run_tests.ps1 test_decklink`. Expected: FAIL (decklink.h not found).
- [ ] **Step 3: Implement `decklink.h/.cpp`** as specified in Interfaces.
  - `linked`: `heardAt != 0 && (uint32_t)(now - heardAt) < LINK_MS`.
  - `Tracker::update`: when both the link and the GPS changed in one pass, return the link message
    and leave the GPS one for the next pass (don't record the GPS state yet).
- [ ] **Step 4: Run the tests and check they pass.** Same command. Expected: `PASS test_decklink`.
- [ ] **Step 5: Commit.** Message: `feat(decks): shared deck-link and GPS state rules`.

### Task 2: `deckmenu` rects for layout B and the bottom prompt line (pure)

**Files:**
- Modify: `src/util/yellow/deckmenu.h` (the comments and `HEADER` / `CLOCK_BAND` /
  `LIGHTS_BAND` / `PROMPT_BAND` / `STATUS` / `LEVEL`), `src/util/yellow/deckmenu.cpp`
  (`displayOnly`, `rect`)
- Test: `test/test_deckmenu/test_deckmenu.cpp`

**Interfaces:**
- Produces (the values the drawing tasks use):
  - **All screens:** `PROMPT_BAND{0, 219, 320, 21}`, display-only on every screen. The prompt
    line's MID rule is its first row, y 219.
  - **Menu:**
    - `rect(Menu, M_ITEM0 + LIGHTS) = {4, 58, 312, 86}`. That is the lights panel, now a button.
    - `rect(Menu, M_ITEM0 + KARAOKE) = {4, 150, 154, 64}`.
    - `rect(Menu, M_ITEM0 + MUSIC) = {162, 150, 154, 64}`.
    - `HEADER{0,0,320,22}` and `CLOCK_BAND{0,22,320,30}` are unchanged.
    - `LIGHTS_BAND` is removed (the panel is the LIGHTS rect).
  - **Lights:**
    - The tiles are `{4 + (i%4)*79, 42 + (i/4)*66, 75, 62}`.
    - `L_MINUS{4,176,96,38}`, `L_PLUS{220,176,96,38}` and `LEVEL{106,176,108,38}`.
    - `STATUS` is removed.
  - **Presets:** `{4 + (i%3)*106, 48 + (i/3)*84, 100, 72}`.
  - **Display-only:** on the Menu, `HEADER`, `CLOCK_BAND` and `PROMPT_BAND`; on Lights, `LEVEL`
    and `PROMPT_BAND`; on Presets, `PROMPT_BAND`.

- [ ] **Step 1: Update the tests.**
  - `test_menu_bands_stack`: HEADER, CLOCK_BAND, the LIGHTS rect, the switch row and PROMPT_BAND
    stack top to bottom without overlap. The switches' bottom is ≤ 214 (a 4 px gap above the rule).
  - `test_menu_ids_and_hits`: a tap at (160, 100) hits LIGHTS, (80, 180) hits KARAOKE and
    (240, 180) hits MUSIC. The KARAOKE and MUSIC rects are both ≥ 150x60.
  - Replace `test_lights_band_is_no_hit` with `test_prompt_band_is_no_hit_everywhere`: (50, 230)
    is -1 on Menu, Lights and Presets.
  - `test_menu_gap_snaps`: (80, 147) hits KARAOKE, and (80, 146) hits LIGHTS (the nearer one;
    Chebyshev ties go to the first id).
  - `test_neighbour`:
    - from LIGHTS, down goes to KARAOKE;
    - from KARAOKE, up goes to LIGHTS and right goes to MUSIC;
    - from MUSIC, up goes to LIGHTS and left goes to KARAOKE.
    - Keep the Lights / Presets cases and update their expected ids to the new rects. The middle
      tiles must still reach `-` / `+` going down.
  - `test_lights_tiles_fill_the_body`: the tiles start at y ≥ 40 (clear of the tabs and their
    focus ring) and the level row ends at ≤ 214.
  - `test_presets_tab`: the six presets sit inside y 40..214 and are each ≥ 60 tall.
  - `check_screen`: compare against `PROMPT_BAND` instead of `STATUS`.
- [ ] **Step 2: Run them and check they fail.** Run `… run_tests.ps1 test_deckmenu`. Expected:
  FAIL.
- [ ] **Step 3: Change `deckmenu.h/.cpp`** to the values above. Rewrite the header comments to
  describe layout B and the prompt line at the bottom.
- [ ] **Step 4: Run them and check they pass.** Run `… run_tests.ps1 test_deckmenu`. Expected:
  PASS. Other suites that include `deckmenu.h` (`test_termfont`) may fail until Task 3; that's
  expected.
- [ ] **Step 5: Commit.** Message: `feat(yellow): layout B and bottom prompt-line rects`.

### Task 3: `termlayout` positions for the prompt line, the panel and the tags (pure)

**Files:**
- Modify: `src/yellow/ui/termlayout.h`
- Test: `test/test_termfont/test_termfont.cpp`

**Interfaces:**
- Consumes: Task 2's rects.
- Produces (in `termlayout`):
  - **Prompt line:** `PROMPT_RULE_Y = 219`, and `PROMPT_BASE`, the SMALL baseline with the caps
    centred between the rule and y 239 (tune so `test_prompt_line_fits` passes, ~234).
  - **Removed:** `LIGHTS_RULE_Y` is no longer used by the prompt line. `STATUS_TEXT_X` is removed.
  - **Lights panel**, inside `rect(Menu, LIGHTS)`:
    - `PANEL_SPLIT = 160` (the vertical rule's x);
    - `LIGHT_BLOCK_W = 34`, `LIGHT_BLOCK_H = 52`, `LIGHT_BLOCK_Y = 70`, `LIGHT_LABEL_Y = 70`,
      `LIGHT_VALUE_BASE = 116`;
    - `lightX(i)`: 12 for inside, `PANEL_SPLIT + 8` for outside;
    - `PANEL_HINT = "LIGHTS >"` (SMALL, DIM), right-aligned at `DASH_RIGHT - 4`, its caps' bottom
      4 px above the panel's bottom.
  - **Switch tags:** the `ON` / `OFF` tags keep `TAG_*`. `ROW_TEXT_Y` becomes the caps top that
    centres BODY in 64 px. `MENU_ITEMS` becomes `{"LIGHTS", "KARAOKE", "MUSIC MODE"}`; LIGHTS's
    label is the hint now, so index 0 is only for the log.
  - **Link tags:**
    - `TAG_GPS = "GPS"`, `TAG_DECK = "DECK"`, `LINK_TAG_GAP = 6`;
    - `int linkTagsW()`: the width of "GPS", the gap and "DECK";
    - `int linkTagsX(bool pad)`: the tags' left x. That is `DASH_RIGHT - linkTagsW()` without a
      pad, else `DASH_RIGHT - padWidth() - 2 * cell(SMALL) - linkTagsW()`.
    - The strike-through is a 1 px line at the caps' middle; drawing is Task 6.

- [ ] **Step 1: Update the tests.**
  - `test_prompt_line_fits` (replaces `test_dash_prompt_fits` and `test_status_fits`):
    - Every message the old two tests listed fits `DASH_X + advance + cell ≤ DASH_RIGHT`, plus:
      - the four decklink messages;
      - `"LINK 2/2"` and `">OUTSIDE: NO LINK"` (the Lights idle line now carries a `>`);
      - `">TAP EACH CROSS WITH THE PEN"`, `">CALIBRATED"`, `">MISSED A CROSS - AGAIN"`.
    - `PROMPT_BASE - accentAscent(SMALL) > PROMPT_RULE_Y` and `PROMPT_BASE + descent(SMALL) ≤ 240`.
    - `PROMPT_RULE_Y == deckmenu::PROMPT_BAND.y`.
  - `test_song_colours_fit`: keep it, against the new `PROMPT_BAND`.
  - `test_dash_lights_fit`:
    - `"PURPLE 100%"` in TITLE fits from `lightTextX(i)` to the panel's half edge (`PANEL_SPLIT - 4`
      for inside, the panel's right edge minus 4 for outside);
    - the block and the value sit inside the panel rect;
    - the hint sits inside the panel and below the values' baseline.
  - `test_menu_items_fit`: `KARAOKE` and `MUSIC MODE` in BODY, with a tag, fit 154x64.
  - `test_link_tags_fit`: for `pad` false and true, the tags start ≥ 8 cells right of the title's
    end (as the pad test does), and the right edge with a pad is ≤ `DASH_RIGHT - padWidth()`.
  - `test_calibration_text_fits`: `"TOUCH CALIBRATION"` in TITLE fits within 320 - 16.
- [ ] **Step 2: Run them and check they fail.** Run `… run_tests.ps1 test_termfont`. Expected:
  FAIL (missing names).
- [ ] **Step 3: Change `termlayout.h`** to the values above. Tune the pixel values until the tests
  pass; the tests, not these numbers, are the contract.
- [ ] **Step 4: Run all tests.** Run `… run_tests.ps1`. Expected: everything PASSes.
- [ ] **Step 5: Commit.** Message: `feat(yellow): termlayout for the bottom prompt line, panel and link tags`.

### Task 4: Yellow deck, one prompt line on every screen (Lights, PRESETS, calibration)

**Files:**
- Modify: `src/yellow/ledpanel.cpp`: the message block (~806-843), the prompt and cursor
  (~955-1013), `drawMessage`, `drawLightState`, `drawAll`, `drawLevel`, `calibrate` and
  `drawTarget`, and the `CAL_SY` values.

**Interfaces:**
- Consumes: `PROMPT_BAND`, `PROMPT_RULE_Y`, `PROMPT_BASE`; `lightstatus::link`.
- Produces (used by Tasks 5 and 6):
  - `static void drawPrompt()`: clears `PROMPT_BAND`, draws the MID rule at `PROMPT_RULE_Y`, then
    the line for the current screen, then the cursor. While a message shows, the message. Idle:
    - Menu: music mode's line or `>`, as today;
    - Lights and Presets: `>` plus `lightstatus::link`'s text, ALERT when down.
  - `static void drawPromptText(const char* s, uint16_t ink)`: the low-level drawer (rule, text,
    cursor) that `drawPrompt` and `calibrate` share.

- [ ] **Step 1:** Generalise `drawPrompt` as above. The cursor uses `PROMPT_BASE`. `blinkCursor`
  runs on every screen.
- [ ] **Step 2:** Delete `drawStatus`. `drawMessage()` and `drawLightState()` call `drawPrompt()`
  on Lights / Presets; `drawLightState()` also calls `drawLightsBand()` on the Menu. `drawAll()`
  draws the prompt line on every screen.
- [ ] **Step 3:** In `drawLevel`, fit the bar and the % into the 38 px `LEVEL`: the bar near the
  top, the % (TITLE, or SMALL for `IN 60%  OUT 40%`) under it, nothing outside the rect.
- [ ] **Step 4:** Restyle `calibrate()` in termpal:
  - `clearBg` the whole screen;
  - `TOUCH CALIBRATION` in TITLE, TEXT, centred at about y 100;
  - the waiting cross in ALERT and a tapped one in TEXT;
  - `drawPromptText` shows `>TAP EACH CROSS WITH THE PEN`, then `>CALIBRATED` (TEXT, 800 ms) or
    `>MISSED A CROSS - AGAIN` (ALERT, 1500 ms).
  - Set `CAL_SY = {24, 24, 196, 196, 120}`. Keep `waitTap`, the fit, the 25 px check and NVS
    exactly as they are.
- [ ] **Step 5: Build.** Run `pio run -e ledpanel`. Expected: SUCCESS. Note the flash % and RAM.
- [ ] **Step 6: Run all tests.** Run `… run_tests.ps1`. Expected: all PASS.
- [ ] **Step 7: Commit.** Message: `feat(yellow): the prompt line at the bottom of every screen`.

### Task 5: Yellow deck, the main screen in layout B

**Files:**
- Modify: `src/yellow/ledpanel.cpp`: `drawButtonBody` (the Menu branch), `drawRow`,
  `drawLightsBand`, `drawDash` and `homeFocus` (if it names items).

**Interfaces:**
- Consumes: Task 2's Menu rects; Task 3's panel values; `drawPrompt`.

- [ ] **Step 1: Draw the LIGHTS item as the panel.** `drawButtonBody(M_ITEM0 + LIGHTS, pressed)`:
  - **Pressed:** `drawBox` gives the solid bar with the ring, as the other buttons do.
  - **Not pressed:** a 1 px MID outline, the vertical MID rule at `PANEL_SPLIT`, each light as
    `drawLightsBand` draws it today (block, label, value) at the new positions, and the
    `PANEL_HINT` in DIM.
  - **Focused (pad):** the outline is 2 px in TEXT and the hint is an inverted tag (like `ON`)
    instead of DIM text. The colours stay readable. This deliberately departs from the spec's
    "inverted bar": an inverted panel would hide the lights for as long as a pad is connected.
    Flag it to João at the desk check.
- [ ] **Step 2:** `drawLightsBand()` now just redraws that button (`drawButton(M_ITEM0 + LIGHTS)`),
  so every look or link change keeps the outline and the focus.
- [ ] **Step 3:** In `drawRow`, centre the label vertically (`ROW_TEXT_Y`); the tags stay bottom
  right. `drawDash` drops the lights' rule (the panel has its outline) and draws the prompt line.
- [ ] **Step 4: Build.** Run `pio run -e ledpanel`. Expected: SUCCESS.
- [ ] **Step 5: Commit.** Message: `feat(yellow): main screen in layout B, the lights panel opens Lights`.

### Task 6: Yellow deck, the link check, the header tags and the messages

**Files:**
- Modify: `src/yellow/ledpanel.cpp`:
  - `deckAnswered`, `deckTask`, `isAlertMsg`;
  - the header: `drawPadBattery` becomes `drawHeaderRight`;
  - `karaokeAnswerTick` and `irSay`, so they record `>BLACK DECK NOT ANSWERING`'s time;
  - `loop()`.

**Interfaces:**
- Consumes: `decklink::*` (Task 1), `linkTagsX`/`TAG_*` (Task 3), `setMessage` / `drawMessage`.

- [ ] **Step 1: Record every valid answer.** In `deckAnswered(bool valid)`, when valid, set
  `deckHeardAt = millis() | 1` (a `static volatile uint32_t`; one 32-bit store is atomic on the
  ESP32).
- [ ] **Step 2: Ask `/link` from the deck task.** Every `decklink::CHECK_MS`, `linkGet()` calls
  `deckGet("/link", nullptr, 0, reply, 16)`, skipped while `ota::active()`.
  - On 200, set `deckGps = decklink::parse(reply)` (a `static volatile uint8_t`) and call
    `deckAnswered(true)`.
  - On any other rc except `NOT_SIGNED`, call `deckAnswered(false)`.
- [ ] **Step 3: Draw the tags.** `drawHeaderRight()` clears from `linkTagsX(true)` to `RULE_X1`
  over the header's rows, then draws the tags at `linkTagsX(padUp)`, then the pad battery as
  today.
  - `GPS` ink: MID for Fix, ALERT plus a 1 px strike for NoFix, DIM for Unknown.
  - `DECK` ink: MID when linked, ALERT plus a strike when not.
  - Every call of `drawPadBattery` becomes `drawHeaderRight`.
- [ ] **Step 4: Update and message each pass.** In `loop()`, every 250 ms:
  - Compute `bool up = decklink::linked(deckHeardAt, now)` and
    `Gps g = decklink::shown(up, (Gps)deckGps)`.
  - If `(up, g)` differs from what's drawn and the screen is the Menu, call `drawHeaderRight()`.
  - Call `tracker.update(up, g, now, notAnsweringAt)`. On a message: `setMessage(m)`, then
    `drawMessage()`.
  - `notAnsweringAt` is set (`millis() | 1`) wherever `>BLACK DECK NOT ANSWERING` is shown.
  - `isAlertMsg` also returns `decklink::isAlert(m)`.
- [ ] **Step 5: Build.** Run `pio run -e ledpanel`. Expected: SUCCESS. Note flash % and RAM.
- [ ] **Step 6: Commit.** Message: `feat(yellow): GPS and DECK tags from a 5 s link check`.

### Task 7: Black deck, `/link`, the link icon and karaoke's tags

**Files:**
- Modify: `src/black/net/remote.h/.cpp`, `src/black/ui/icon_map.h/.cpp`,
  `src/black/ui/camband.h/.cpp`, `src/black/ui/karaokeview.h/.cpp`, `src/black/main.cpp`
- Test: `test/test_icon_map/test_icon_map.cpp`

**Interfaces:**
- Consumes: `decklink::body`, `decklink::linked`.
- Produces:
  - `uint32_t remote::yellowHeardAt();`, where 0 means never.
  - `icons::Icon::Link` and `icons::Icon::LinkOff`, appended after `GpsOff`.
  - `camband::draw(TFT_eSPI&, const cameras::Status&, const camlist::Point*, bool mute, bool linked,
    bool strip)`. With `strip` false (karaoke) it draws no top-strip icons.
  - `void ui::drawKaraokeTags(TFT_eSPI&, bool fix, bool linked);`

- [ ] **Step 1: Write the failing test.** In `test_icon_map`, add `test_link_icons`:
  - `Link` and `LinkOff` are 12x12 (covered by `test_every_icon_is_12x12` once they're in the
    enum);
  - they differ;
  - `pixel(LinkOff, 0, 0)` and `pixel(LinkOff, 11, 11)` are true (the strike runs corner to
    corner, like `GpsOff`'s);
  - `pixel(Link, 0, 0)` is false.
- [ ] **Step 2: Run it and check it fails.** Run `… run_tests.ps1 test_icon_map`. Expected: FAIL.
- [ ] **Step 3: Draw the icon.** Add the art: two 4x4 screens on stands joined by a cable, and the
  same with a TL→BR strike. Use the spec's mockup as a start and clean it up so the cable reads at
  12 px.
- [ ] **Step 4: Run it and check it passes.** Run `… run_tests.ps1 test_icon_map`. Expected: PASS.
- [ ] **Step 5: Answer `/link` in `remote.cpp`.**
  - `handleLink()`: `reqauth::pass(*s_server, s_gate, "/link", "wrong key")`, then
    `noteYellow()`, then `camsvc::snapshot`. `fix` is `stage != cameras::Stage::NoFix`. Send 200
    `text/plain` with `decklink::body`. Register it beside `/karaoke`.
  - `noteYellow()` stores `millis() | 1` in a `std::atomic<uint32_t>`. Call it after the auth
    passes in `handleMusic` and `handleKaraoke` too, and **not** in the playlist handlers.
- [ ] **Step 6: Draw the pair in `camband.cpp`.** `tag(bool fix, bool linked)` draws the GPS icon
  and the link icon centred as a pair in `ui::topGap`, 4 px apart.
  - `s_shown` gains `linked`, so a link change in Off / NoFix redraws the tag.
  - With `strip` false, nothing is drawn, but the shown state is still updated, so the strip gets
    the tag again when karaoke ends (a whole redraw).
- [ ] **Step 7: Draw karaoke's tags in `karaokeview.cpp`.** `drawKaraokeTags` puts `GPS` and
  `DECK` right-aligned at `TEXT_X + TEXT_W` on `SONG_Y`, in the view's small face:
  - `GPS` dim with a fix, alert colour plus a strike without one;
  - `DECK` dim when linked, alert colour plus a strike when not.

  It uses the karaoke palette's dim/alert colours (`util/black/karaokepal`); if there's no alert
  entry, use termpal's ALERT. `drawKaraoke(whole)` cuts the artist's max width by the tags' width
  plus `SONG_GAP`.
- [ ] **Step 8: Wire it up in `main.cpp`.** Compute `linked = decklink::linked(remote::yellowHeardAt(),
  millis())` with `cam`.
  - Pass `linked` and `strip = (shownKaraoke == Normal)` to `camband::draw`.
  - While karaoke shows, call `ui::drawKaraokeTags(s_tft, fix, linked)` after a whole draw and
    whenever `(fix, linked)` changes, inside the current viewport, so a camera panel clips it.
- [ ] **Step 9: Build and test.** Run `pio run -e s3`, then `… run_tests.ps1`. Expected: SUCCESS,
  all PASS.
- [ ] **Step 10: Commit.** Message: `feat(black): /link for the yellow deck, a link icon by the GPS pin, karaoke tags`.

### Task 8: Build both, flash the yellow deck, desk check

- [ ] **Step 1: Build both.** Run `pio run -e ledpanel -e s3`. Expected: both SUCCESS. Record the
  ledpanel flash % (it must be under 100 %).
- [ ] **Step 2: Flash the yellow deck.** Run `pio run -e ledpanel -t upload --upload-port COM16`.
  Then watch the serial: `[mem]` lowest free should stay ≳ 30 KB online.
- [ ] **Step 3: Ask João before the black deck.** Ask whether to flash it (`pio run -e s3 -t upload
  --upload-port COM12`). Flash only on his yes, with any capture stopped first.
- [ ] **Step 4: Desk check with João.** He confirms:
  - the prompt line is at the bottom on the main screen, all four Lights tabs and calibration
    (BOOT held 2 s);
  - the lights panel opens Lights;
  - the switches flip;
  - the pad's focus moves across the three targets;
  - `DECK` goes red with `>DECK: NO LINK` within ~15 s of unplugging the black deck, and back with
    `>DECK: LINKED`;
  - `GPS` goes red with `>GPS: NO FIX` when GPS 2 IP stops (needs the hotspot);
  - the panel's focus look (Task 5's deviation) is acceptable.

### Task 9: Docs

**Files:** `docs/yellow-deck.md` ("On the deck"), `docs/ARCHITECTURE.md` (Between the devices:
`/link` and the 5 s / 15 s rule; Black deck: the link icon and karaoke tags), `CHANGELOG.md`
(Unreleased: Yellow deck, Black deck and Both decks entries in the existing voice).

- [ ] **Step 1: Update `docs/yellow-deck.md`.**
  - The main-screen bullets become layout B and the bottom prompt line.
  - Lights loses its "Status line" bullet: the link text is now the prompt line's idle text.
  - Add the `GPS` / `DECK` tags and their messages.
  - Calibration gets the new look.
- [ ] **Step 2: Update `docs/ARCHITECTURE.md` and `CHANGELOG.md`.**
- [ ] **Step 3: Commit.** Message: `docs: bottom prompt line, layout B, GPS and DECK link`.
- [ ] **Step 4: Suggest `/brain-sync` to João.**
