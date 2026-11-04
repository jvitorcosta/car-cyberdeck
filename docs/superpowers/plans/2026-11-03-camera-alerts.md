# Camera Alerts Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** GPS alerts and alarms for Manaus speed cameras, electronic speed bumps and red-light cameras, run by the black deck (GPS, matching, beeps, its own band) and mirrored on the yellow deck through a signed `GET /cam`.

**Architecture:** Pure, host-tested modules in `src/util/car/` (list parsing, NMEA, matching, the `/cam` wire format, beep patterns) and `src/util/yellow/` (voice words); thin device glue in `src/black/` and `src/yellow/ledpanel.cpp`. The list is a CSV kept in the repo, curated by `tools/cameras.py`, pushed to the black deck over WiFi and stored in its NVS.

**Tech Stack:** C++17, Arduino-ESP32 on PlatformIO (envs `s3`, `ledpanel`), Unity host tests run by `tools/run_tests.ps1`, Python 3 + pytest for the tool, Leaflet for the placing map.

**Spec:** `docs/superpowers/specs/2026-11-03-camera-alerts-design.md`

## Global Constraints

- Nothing senses radar or any signal: GPS position against a list only (CTB art. 230, III).
- Three kinds: `speed`, `bump`, `redlight`. CSV columns, in order: `id,kind,lat,lon,heading,limit,label,source,checked`.
- `heading` is required on every row the deck loads; `limit` is empty for `redlight`, and for other kinds only while unknown (unknown → alerts with `--`, never alarms).
- Matching: no alerts with no fix or under 10 km/h; `nofix` after 5 s without a valid fix, moving or not (the clock starts at the first update: the boot grace); candidates within 800 m; course within ±40° of `heading`; bearing to the point within ±30° of course; inside 200 m, cross-track under 30 m; passed = was within 60 m and the distance grows, then ignored for 60 s; nearest remaining point wins.
- Stages: alert at max(400 m, 20 s × speed) for speed/bump, 250 m for red-light; alarm zone is the last 200 m, speed (km/h rounded down) > limit, no tolerance; red-light never alarms; the band clears 2 s after passing.
- Sounds on the black deck only. Since Task 10: chime + Japanese voice lines per the spec's "Sound" table, the alarm's triple beep every 1.5 s until clear, and the old beeps (speed = two mid tones, bump = two low, red-light = one long high) as the fallback when a clip is missing. Alerts interrupt the intro / music box, which then continue.
- Mute keeps the bands, silences the chime and voice (since Task 10 the alarm's triple beep still plays), lives on the black deck, resets at each start; reached by voice ("cameras mute"), the watch (it sends the same phrase to `/say`) and the yellow deck's touch.
- Speed is GPS only in this version; the GPS is the owner's iPhone running GPS 2 IP (NMEA over a TCP socket on the hotspot, about 1 Hz), not a module.
- The black deck's Spotify, lyrics and self-heal numbers must not get worse (largest heap block never under 102 KB in the 30-min check).
- Comments and names follow the repo: a short header comment per module, `// PURE, host-tested` on pure ones, namespaces in lower case.
- Commits end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

- A GPS that reports a valid fix with course 0 while stopped, then moving: course is unreliable under 10 km/h, so nothing may alert from it (Task 2 test `test_slow_course_is_ignored`).
- Two cameras close together in a row (Av. do Turismo has several): after passing the first, the second must alert without waiting 60 s (Task 2 test `test_next_point_after_passing`).
- A pushed list with a bad line, a missing heading or more rows than fit: the deck keeps the previous list and the push gets a 400 naming the line (Task 1 `test_missing_heading_rejects_list`, Task 5 step).
- Headings near north (350° vs 10°): the angle maths must wrap (Task 2 `test_heading_wraps_at_north`).
- The black deck rebooting mid-drive: the list comes back from NVS and mute resets to off (Task 5 step).

---

### Task 1: The camera list (CSV → points)

**Files:**
- Create: `src/util/car/camlist.h`, `src/util/car/camlist.cpp`
- Test: `test/test_camlist/test_camlist.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace camlist {
  enum class Kind : uint8_t { Speed, Bump, RedLight };
  constexpr int MAX_POINTS = 96;
  constexpr size_t MAX_CSV = 6144;          // what the deck stores and accepts (NVS headroom)
  constexpr uint8_t NO_LIMIT = 0;
  struct Point { int32_t latE6, lonE6; uint16_t heading; uint8_t limit; Kind kind; char id[12]; char label[28]; };
  // Parses the whole CSV (header line required, exactly the column order above; CRLF or LF).
  // Returns the row count, or -1 with *badLine = 1-based line of the first bad row (header = 1).
  // Bad: wrong column count, unknown kind, lat/lon not a number, heading empty or not 0..359,
  // limit not empty/1..199, a redlight with a limit, more than max rows. Label cut to fit, UTF-8 safe.
  int parse(const char* csv, size_t len, Point* out, int max, int* badLine);
  const char* kindName(Kind k);             // "speed" | "bump" | "redlight"
  }
  ```

- [ ] **Step 1: Write the failing tests** in `test/test_camlist/test_camlist.cpp`:
  `test_parses_three_kinds` (one row each; `latE6 == -2965693`, `lonE6 == -60021184`, heading 270, limit 60 / 40 / `NO_LIMIT`),
  `test_crlf_and_trailing_newline`, `test_missing_heading_rejects_list` (returns -1, `badLine == 3`),
  `test_redlight_with_limit_is_bad`, `test_unknown_limit_is_no_limit` (speed row with empty limit → `NO_LIMIT`),
  `test_too_many_rows` (`max = 2`, three rows → -1, `badLine == 4`),
  `test_label_cut_keeps_utf8` (a 40-byte label with "ç" across byte 27 → no half character, NUL-terminated).
- [ ] **Step 2: Run** `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1` → `FAIL test_camlist` (does not compile).
- [ ] **Step 3: Implement** `camlist.h/.cpp` to the interface above (degrees parsed to micro-degrees with integer maths, no `atof` rounding drift: `-2.9656927` → `-2965693`, round half away from zero).
- [ ] **Step 4: Run** the suites → `PASS test_camlist`, every other suite still passes.
- [ ] **Step 5: Commit** `feat(car): camera list parser`.

### Task 2: Matching (fix → stage)

**Files:**
- Create: `src/util/car/cameras.h`, `src/util/car/cameras.cpp`
- Test: `test/test_cameras/test_cameras.cpp`

**Interfaces:**
- Consumes: `camlist::Point`, `camlist::Kind`, `camlist::NO_LIMIT` (Task 1).
- Produces:
  ```cpp
  namespace cameras {
  struct Fix { bool valid; int32_t latE6, lonE6; uint16_t kmh; uint16_t course; uint32_t ms; };
  enum class Stage : uint8_t { Off, NoFix, Alert, Alarm };
  struct Status { Stage stage = Stage::Off; int index = -1; uint16_t distM = 0; uint16_t kmh = 0; };
  class Matcher {
  public:
      void load(const camlist::Point* pts, int n);   // keeps the pointer; clears passed state
      Status update(const Fix& f);                   // call per fix (about 1 Hz) and at least 1 Hz without one
  };
  // Geometry helpers, exposed for tests: metres, degrees 0..359, signed metres (left < 0).
  uint32_t distanceM(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2);
  uint16_t bearing(int32_t lat1, int32_t lon1, int32_t lat2, int32_t lon2);
  int angleDiff(int a, int b);                       // -179..180
  int32_t crossTrackM(const Fix& f, const camlist::Point& p);
  }
  ```
  All thresholds as `constexpr` at the top of `cameras.h` with the Global Constraints' values. A linear scan of ≤96 points per fix (no grid: the spec left it to the builder, and 96 points a second is nothing). Fixes arrive about once a second, so a car at 100 km/h moves 28 m between them: every threshold must hold with that step. Equirectangular maths with `cos(lat)` of the fix.

- [ ] **Step 1: Write the failing tests.** Build fixtures with a helper `Fix along(int32_t lat, int32_t lon, uint16_t course, int metresBefore, uint16_t kmh, uint32_t ms)` placing the car `metresBefore` metres before a point along `course`. Tests:
  - `test_alert_at_400m` (60 km/h, speed point limit 60: 410 m → `Off`, 390 m → `Alert`, `distM` within ±5 of 390).
  - `test_alert_reaches_further_when_fast` (100 km/h: 20 s = 556 m → `Alert` at 540 m).
  - `test_redlight_alerts_at_250m_never_alarms` (90 km/h at 100 m → `Alert`).
  - `test_alarm_over_limit_in_last_200m` (61 km/h at 190 m → `Alarm`; 60 km/h → `Alert`; 61 km/h at 210 m → `Alert`).
  - `test_unknown_limit_never_alarms`.
  - `test_opposite_lane_silent` (course 180° off the heading → `Off`).
  - `test_parallel_road_silent` (car 40 m to the side, same course, 150 m before → `Off`; 20 m to the side → `Alert`).
  - `test_overpass_silent` (point 35 m sideways at 100 m → `Off`).
  - `test_passed_clears_and_waits_60s` (drive through: within 60 m then 80 m past → `Off`; turn round and approach again at +30 s → still `Off` for that point; at +61 s → `Alert`).
  - `test_next_point_after_passing` (two points 500 m apart on one road: right after passing the first, the second alerts).
  - `test_nofix_after_5s_moving` (valid fixes at 50 km/h, then invalid ones: 4.9 s → previous stage, 5.1 s → `NoFix`).
  - `test_slow_course_is_ignored` (9 km/h pointing at the point at 100 m → `Off`).
  - `test_heading_wraps_at_north` (heading 355, course 10 → `Alert`).
- [ ] **Step 2: Run** the suites → `FAIL test_cameras`.
- [ ] **Step 3: Implement** to the interface; passed state per point index as the `ms` it was passed (0 = not), and a per-point "was within 60 m" flag; `Stop` (under 10 km/h) keeps `NoFix` / passed state as they are but returns `Off`.
- [ ] **Step 4: Run** → `PASS test_cameras`.
- [ ] **Step 5: Commit** `feat(car): camera matching by position, course and cross-track`.

### Task 3: NMEA reader

**Files:**
- Create: `src/util/car/nmea.h`, `src/util/car/nmea.cpp`
- Test: `test/test_nmea/test_nmea.cpp`

**Interfaces:**
- Consumes: `cameras::Fix` (Task 2).
- Produces:
  ```cpp
  namespace nmea {
  // Feeds bytes one at a time; true when a complete $..RMC sentence with a good checksum was read,
  // and *out then holds it (valid = status 'A'; knots → km/h rounded down; course empty → previous).
  class Reader { public: bool feed(char c, uint32_t ms, cameras::Fix* out); };
  // The UBX/PMTK-free setup lines sent at boot are the builder's (see Task 5); not here.
  }
  ```
  Own parser (the spec allows TinyGPS++; ours is ~100 lines, has no allocation, and accepts `$GPRMC`, `$GNRMC`, `$GARMC`).
- [ ] **Step 1: Write the failing tests:** `test_valid_rmc` (`$GNRMC,123519.00,A,0257.94156,S,06001.27103,W,32.40,270.0,031126,,,A*xx` with the right checksum → `latE6 == -2965693`, `lonE6 == -60021184` ±1, `kmh == 60`, `course == 270`), `test_void_rmc_is_not_valid`, `test_bad_checksum_ignored`, `test_split_across_feeds`, `test_overlong_line_resets` (300 chars without `\n`, then a good sentence → read).
- [ ] **Step 2: Run** → `FAIL test_nmea`.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** → `PASS test_nmea`.
- [ ] **Step 5: Commit** `feat(car): NMEA RMC reader`.

### Task 4: The `/cam` wire format and beep patterns

**Files:**
- Create: `src/util/car/camwire.h`, `src/util/car/camwire.cpp`, `src/util/car/camtone.h`, `src/util/car/camtone.cpp`
- Test: `test/test_camwire/test_camwire.cpp`, `test/test_camtone/test_camtone.cpp`

**Interfaces:**
- Consumes: `cameras::Status`, `cameras::Stage`, `camlist::Point` (Tasks 1-2).
- Produces:
  ```cpp
  namespace camwire {
  constexpr size_t MAX_JSON = 192;
  // {"st":"off|nofix|alert|alarm","kind":"speed","limit":60,"dist":320,"kmh":58,"label":"…","id":"mao-0007","mute":0}
  // kind/limit/dist/label/id only when st is alert or alarm; limit 0 = unknown or red-light.
  bool write(const cameras::Status& s, const camlist::Point* active, bool mute, char* out, size_t n);
  struct View { cameras::Stage stage; camlist::Kind kind; uint8_t limit; uint16_t distM, kmh; bool mute; char label[28]; };
  bool parse(const char* json, View* out);            // false on anything it can't read
  }
  namespace camtone {
  struct Note { uint16_t hz; uint16_t ms; };          // hz 0 = rest
  constexpr int MAX_NOTES = 6;
  constexpr uint32_t ALARM_REPEAT_MS = 1500;
  // The notes to play when the stage changes from `from` to `to` for a point of `kind`
  // (alert: per kind; alarm: the triple beep; anything else: none). Returns the count.
  int onChange(cameras::Stage from, cameras::Stage to, camlist::Kind kind, Note* out);
  int alarmRepeat(Note* out);                         // the triple beep again
  }
  ```
  Tones (our call, the spec left exact tones to the builder): mid = 1000 Hz, low = 600 Hz, high = 1600 Hz; alert beeps 120 ms with 80 ms rests; red-light 500 ms; alarm 3 × 80 ms at 1600 Hz with 60 ms rests.
- [ ] **Step 1: Write the failing tests:** `test_write_alert_round_trips` (write then parse equals), `test_off_has_no_point_fields` (`strstr(out, "kind") == nullptr`), `test_label_with_quote_escaped`, `test_parse_rejects_garbage`; `test_speed_alert_two_mid`, `test_bump_alert_two_low`, `test_redlight_one_long_high`, `test_alert_to_alarm_triple`, `test_alarm_to_alert_silent`, `test_alert_to_off_silent`.
- [ ] **Step 2: Run** → both suites fail.
- [ ] **Step 3: Implement** (reuse `util/common/jsonflat` / `jsonpath` for parsing, as `stateparse` does).
- [ ] **Step 4: Run** → both pass.
- [ ] **Step 5: Commit** `feat(car): /cam wire format and beep patterns`.

### Task 5: Black deck: GPS, list storage and `/cam`

**Files:**
- Create: `src/black/car/gps.cpp`, `src/black/car/gps.h`, `src/black/car/camsvc.cpp`, `src/black/car/camsvc.h`
- Modify: `src/black/net/remote.cpp` (routes), `src/black/main.cpp` (start the service), `src/config.h.example` or the repo's config template (`GPS_HOST` default `"172.20.10.1"`, `GPS_PORT` default `11123`)
- Modify: `docs/superpowers/specs/2026-11-03-camera-alerts-design.md` (the list lives in NVS, not the media partition: the pack is read-only and replaced only by `uploadmedia`, while `push` must store it without a reboot)

**Interfaces:**
- Consumes: Tasks 1-4.
- Produces:
  ```cpp
  namespace camsvc {
  void begin();                         // NVS → list, starts the GPS task (TCP client to GPS_HOST:GPS_PORT)
  void snapshot(cameras::Status* s, camlist::Point* active, bool* mute);   // thread-safe copy
  bool setList(const char* csv, size_t len, int* badLine);  // parse, then store in NVS ("cam"/"list"); old list kept on failure
  void setMute(bool on);                // RAM only: resets at each start
  bool mark();                          // appends the last valid fix to NVS ("cam"/"marks"), max 32; false without a fix
  size_t marksCsv(char* out, size_t n); // "ms,lat,lon,course,kmh\n" rows
  void clearMarks();
  void onStage(void (*cb)(cameras::Stage from, cameras::Stage to, camlist::Kind kind));  // for Task 6's sound
  }
  ```
  Routes (all in `remote.cpp`, beside `/state`):
  - `GET /cam` → `camwire::write` (signed like `/state`, through `reqauth::pass`).
  - `GET /cam/mute?on=1|0` and `GET /cam/mark` → signed, 204 / 409 "no fix" / 507 "not stored" (mark: NVS, or marks it could not read back; it never overwrites them).
  - `POST /cam/list?k=` body = CSV (the PC tool, bearer key like `/trip/reset`; 413 over `MAX_CSV`; 400 `line N` from `badLine`).
  - `GET /cam/marks?k=` → `marksCsv`; `POST /cam/marks/clear?k=` → 204.
- [ ] **Step 1: Config.** Add `GPS_HOST` / `GPS_PORT` where the other deck addresses live (`config.h` is git-ignored: update its committed template and the setup docs).
- [ ] **Step 2: Implement `gps.cpp`:** a task (4 KB stack, priority 1, core 0) that keeps a `WiFiClient` to `GPS_HOST:GPS_PORT` (connect timeout 1 s; back-off 2 s → 30 s while it fails; reconnect when no byte arrives for 5 s) and feeds every byte to `nmea::Reader`; only while WiFi is up; one plain TCP socket, no TLS (it must not touch the Spotify TLS heap).
- [ ] **Step 3: Implement `camsvc.cpp`** to the interface: list in an NVS blob (`Preferences`, namespace `cam`), a `Matcher` updated per fix and once a second without one, a mutex around the snapshot.
- [ ] **Step 4: Add the routes** in `remote.cpp`.
- [ ] **Step 5: Build** `pio run -e s3` → succeeds; RAM and flash use within 2 % of `main`'s.
- [ ] **Step 6: Bench check on the deck:** add `tools/nmea_replay.py` (a TCP server that sends a recorded or synthesised NMEA file at 1 Hz, `--port 11123`); point `GPS_HOST` at the PC; push a 3-row test CSV with `curl -X POST --data-binary @test.csv` (or Task 8's `push`), and see `/cam` go `off → alert → alarm → off`. Then a bad CSV (row without heading) → 400 `line 3`, and `/cam` still uses the old list. Reboot → the list is still there, `"mute":0`.
- [ ] **Step 7: Update the spec's storage line** (and note that the PC replay stands in for the phone), then **commit** `feat(black): GPS, camera list and /cam`.

### Task 6: Black deck: beeps and band

**Files:**
- Create: `src/black/car/camsound.cpp`, `src/black/car/camsound.h`, `src/black/ui/camband.cpp`, `src/black/ui/camband.h`
- Modify: `src/black/audio/greeting.cpp` (pause / resume hook), `src/black/main.cpp` (draw the band, register `onStage`)

**Interfaces:**
- Consumes: `camsvc::onStage`, `camsvc::snapshot`, `camtone` (Tasks 4-5), `audioport::acquire/release`.
- Produces: `camsound::begin()`; `camband::draw(TFT_eSPI& tft, const cameras::Status&, const camlist::Point*)` drawing over the bottom 48 px of the current screen; `greeting::pauseFor(uint32_t ms)` that silences the playing clip for `ms` and lets it carry on.
- [ ] **Step 1: Implement `camsound`:** a small task with a queue of `camtone::Note` runs; it pauses the greeting, acquires the port at the greeting's rate, writes generated square/sine samples, releases. While the stage stays `Alarm`, it queues `alarmRepeat` every `ALARM_REPEAT_MS`. Nothing plays while muted.
- [ ] **Step 2: Implement `camband`:** icon + `RADAR 60` / `LOMBADA 40` / `SINAL`, distance, speed; red for `Alarm`; `GPS --` corner tag for `NoFix`; nothing for `Off`; redraw only on change (the band must not cost the battle screen's frame rate).
- [ ] **Step 3: Build** `pio run -e s3`.
- [ ] **Step 4: Bench check on the deck** with the Task 5 replay: hear speed / bump / red-light alerts and the alarm repeat; the intro or music box pauses and continues; `/cam/mute?on=1` silences them while the band still shows.
- [ ] **Step 5: Heap check:** 30 min with Spotify playing and the replay looping; `[mem]` log's largest block never under 102 KB.
- [ ] **Step 6: Commit** `feat(black): camera alert sounds and band`.

### Task 7: Yellow deck: band, voice and mute

**Files:**
- Create: `src/util/yellow/camvoice.h` (pure, header-only like `ledvoice.h`)
- Modify: `src/yellow/ledpanel.cpp` (poll task, band, `/say` hook, touch mute)
- Test: `test/test_camvoice/test_camvoice.cpp`

**Interfaces:**
- Consumes: `camwire::parse`, `camwire::View` (Task 4); black deck routes (Task 5); `deckGet` in `ledpanel.cpp`.
- Produces:
  ```cpp
  namespace camvoice {
  enum class Cmd { None, Mark, Mute, Unmute };
  // "mark camera", "marca radar", "cameras mute", "mute cameras", "silenciar radar", "cameras on", "radar on".
  Cmd parse(const char* text);
  }
  ```
- [ ] **Step 1: Write the failing tests:** each phrase above (any case, punctuation), `test_lights_words_are_not_camera_words` ("purple inside" → `None`), `test_camera_alone_is_none`.
- [ ] **Step 2: Run** → `FAIL test_camvoice`. **Step 3: Implement.** **Step 4: Run** → pass.
- [ ] **Step 5: Wire it in `ledpanel.cpp`:**
  - A `cam` poll task (4 KB): `deckGet("/cam", …)` every 1 s at all times; three misses → `CAM --`.
  - The band over the bottom third of the dashboard, LYRICS and PRESETS screens (green alert, orange red-light, red blinking 2 Hz alarm with the speed large), `GPS --` on the dashboard's status row; the band goes 2 s after `st` leaves alert/alarm.
  - `handleSay`: before `ledvoice::parse`, `camvoice::parse`; `Mark` → `deckGet("/cam/mark")` and reply "Marked" / "No GPS fix"; `Mute`/`Unmute` → `/cam/mute?on=` and reply "Camera sounds off" / "Camera sounds on".
  - Touch: a tap on the band toggles mute (the band shows `MUTE` while muted).
- [ ] **Step 6: Build** `pio run -e ledpanel` → succeeds; run the suites → all pass.
- [ ] **Step 7: Bench check** with the Task 5 replay running: the yellow band follows the black deck within about 1 s; "cameras mute" by Siri silences the black deck; the watch's `/say` with "mark camera" answers "Marked" (no watch code change: it already sends free text through `Deck.send`; a menu item for it is a follow-up in `docs/BACKLOG.md`).
- [ ] **Step 8: Commit** `feat(yellow): camera band, voice and mute`.

### Task 8: `tools/cameras.py`

**Files:**
- Create: `tools/cameras.py`, `tools/camera_place.html` (template), `tools/test_cameras_tool.py`
- Create: `data/cameras.csv` (header only for now)

**Interfaces:**
- Consumes: the CSV format (Task 1), the black deck's `/cam/list`, `/cam/marks`, `/cam/marks/clear` (Task 5); `REMOTE_TOKEN` and the deck address read the same way `tools/media_upload.py` reads them.
- Produces the commands:
  - `fetch`: IMMU page (`https://immu.manaus.am.gov.br/index.php?r=site%2Finforadar`, its three lists by heading text) + Overpass (`highway=speed_camera` in `-3.25,-60.15,-2.90,-59.80`, with a User-Agent) → prints new / gone vs `data/cameras.csv` (match: label words or within 50 m). Edits nothing.
  - `place [--from immu|osm|marks]`: for each unplaced item, a Nominatim guess (≤1 request/s, User-Agent set), then writes `build/camera_place.html` with draggable pins, a heading arrow per pin, kind and limit fields (prefilled from the survey or `maxspeed`), and a "save" button that downloads `placed.json`; `place --apply placed.json` merges into `data/cameras.csv` with new `mao-NNNN` ids and today's `checked`.
  - `push`: validates locally with the same rules as `camlist::parse` (heading required, red-light without limit, ≤96 rows, ≤6144 bytes; a list with no rows only with `--force`), then `POST /cam/list`.
  - `import-car`: `GET /cam/marks` → items for `place --from marks`; after `--apply`, `POST /cam/marks/clear`.
  - `check`: `fetch`, exit code 1 when anything is new or gone.
- [ ] **Step 1: Write the failing pytest tests:** `test_validate_rejects_missing_heading`, `test_validate_rejects_redlight_limit`, `test_parse_immu_lists` (a saved copy of the page in `tools/testdata/immu_inforadar.html` → 13 speed, 2 bump, 5 redlight, each with its `sentido`), `test_apply_assigns_next_id`, `test_match_by_distance`.
- [ ] **Step 2: Run** `python -m pytest tools/test_cameras_tool.py` → fails.
- [ ] **Step 3: Implement** (stdlib `urllib`, `html.parser`, `csv`, `json`; no new dependencies beyond `tools/requirements.txt`).
- [ ] **Step 4: Run** → passes.
- [ ] **Step 5: Commit** `feat(tools): cameras.py fetch, place, push, import-car, check`.

### Task 9: The first Manaus list and the drive

**Files:**
- Modify: `data/cameras.csv`, `docs/BACKLOG.md`, `README` / device docs where the decks' features are listed

- [ ] **Step 1 (owner):** `python tools/cameras.py fetch`, then `place --from immu` and `place --from osm`; drag each pin onto its lane and set the arrow using the IMMU direction and the survey photos; limits from the surveys (Av. do Turismo 60) and the road signs, empty where unknown. `place --apply`, commit `data/cameras.csv`.
- [ ] **Step 2:** `python tools/cameras.py push` → 204; with GPS 2 IP closed `/cam` answers `nofix` (both decks `GPS --`) about 5 s after boot. Install GPS 2 IP on the iPhone, set it to socket server mode on port 11123 with background mode on; `/cam` answers `off` once the hotspot is up and the app sends a fix.
- [ ] **Step 3 (owner, in the car):** drive past at least one point of each kind, one in the opposite direction and one parallel road (e.g. Av. do Turismo, both ways). Expected: alerts at the right distance, no alert the other way, alarm only above the limit, both bands agree, music resumes. Note any false alarm with its place.
- [ ] **Step 4:** "mark camera" once on the drive; `import-car` shows it.
- [ ] **Step 5:** Add to `docs/BACKLOG.md`: the watch MARK / MUTE menu items, OBD speed when the OBD deck exists, the bench gates still owed, any false-alarm places from Step 3. **Commit** `docs: camera alerts in the car`.

### Task 10: Voice alerts (chime + Japanese lines)

Added 2026-11-03 after the owner listened to a simulated drive and approved the sounds. Spec: "Sound (black deck speaker)".

**Files:**
- Create: `tools/gen_camvoice.py` (+ pytest in `tools/test_gen_camvoice.py`), `src/util/car/camvoice.h/.cpp` (the clip table's reader, PURE), `src/util/car/camseq.h/.cpp` (what to play for each event, PURE; may replace or wrap `camcue`), host tests under `test/`
- Modify: `src/black/car/camsound.cpp/.h` (play clips as well as tones), `src/black/car/camsvc.*` if the change needs the point's limit or the last speed, `tools/pack_media.py` (add `camvoice.bin`), `docs/BACKLOG.md` (bench gates)

**Interfaces:**
- `data/camvoice.bin` (git-ignored, made by `python tools/gen_camvoice.py <dir with the 16 kHz WAVs>`): a fixed slot table, then 16 kHz mono s16le PCM. Slots, in this order: `PON, GPS_FIRST, TORISHIMARI, GOCHUI, SPD_CHANGE, SPEEDNI, GPS_NG, GPS_OK, L20, L30, L40, L50, L60, L80`. Source names: `chimevoice_chime_pon_16k.wav`, `chimevoice_gps_16k.wav`, `cam_torishimari_02_16k.wav`, `cam_gochuuikudasai_01_16k.wav`, `cam_spd-change_01_16k.wav`, `cam_speednikiwo_01_16k.wav`, `cam_gps-ng_01_16k.wav`, `cam_gps-ok_02_16k.wav`, `cam_speedNNika_01_16k.wav`. A missing WAV is an empty slot. The tool refuses WAVs that are not 16 kHz mono 16-bit.
- The sequencer is fed stage changes with the point's kind, id and limit (carry id and limit in camsound's queued `Change`: fixes Task 6's deferred minor of reading the id at dequeue time), and the GPS events (no fix while driving, fix back, first fix), and returns steps: a clip slot or a tone pattern. It owns: the 20 s short call, the limit-change line, the 3 s chime guard, the alarm cadence (voice on entry, triple beep each 1.5 s, voice again in a beep slot once 6 s passed), priority and dropping stale steps, mute (voice and chime off, alarm beeps on), and the fallback beep when a slot is empty.

- [ ] **Step 1: Write failing host tests** for the sequencer: speed/bump/red-light full calls; limit 70 has no limit line; short call within 20 s, with and without a limit change; red-light short call unchanged; alarm entry voice then beeps, voice again after 6 s, silence when Alarm ends; alert sequence dropped when the stage moves to Alarm or Off mid-way; GPS lost only after a fix at >= 10 km/h, once per loss; fix back plays GPS_OK only after GPS_NG played; first fix plays GPS_FIRST once; mute keeps alarm beeps, drops voice; empty slot falls back to the old beep pattern for that event. Plus `camvoice` table parsing (bad header, slot out of range, empty slot).
- [ ] **Step 2: Run** `tools/run_tests.ps1` → the new tests fail.
- [ ] **Step 3: Implement** the pure modules, then `gen_camvoice.py` (+ pytest), `pack_media.py`, and camsound playing clips through the same `greeting::pauseFor/beginTones/endTones` path as the beeps (resample 16 kHz to the output's rate when a paused clip's output runs at another rate; cut at the next 128-sample write when a higher-priority step arrives).
- [ ] **Step 4: Run** `tools/run_tests.ps1`, `python -m pytest tools`, `pio run -e s3`, `pio run -e ledpanel` → all pass.
- [ ] **Step 5:** BACKLOG bench gates: hear each call over the speaker and over a playing intro; volume against the beeps; `[mem]` 30-min check unchanged.
- [ ] **Step 6: Commit** `feat(black): camera alerts speak (chime + voice, beeps as fallback)`.

### Task 11: Sign panel on the black deck

Added 2026-11-03 after the owner approved a mockup. Spec: "Black deck (the one that knows)", **Sign panel**.

**Files:**
- Modify: `src/black/ui/camband.cpp/.h` (the panel, the new band layout, the countdown bar, the alarm blink), `src/black/main.cpp` (the viewport leaves out the panel's columns while it shows)
- Create if the maths wants tests: a PURE helper in `src/util/car/` (e.g. the countdown bar's width from distance and alert distance, the `+N` over-limit value, the blink phase), host-tested

**Interfaces:**
- `camband` gains a panel rect (`PANEL = {192, 0, 128, 192}` at the 320 × 240 landscape screen) and reports the area the screens must keep out of while it shows (the bottom BAND_H rows and the panel), so main.cpp can set one viewport `(0, 0, 192, 240 - BAND_H)`.
- The countdown bar's full width is the point's alert distance (cameras.*: max(400 m, 20 s × speed) for speed / bump, 250 m for red-light, from when the alert began); it never grows back while the same alert lasts.
- The alarm blink runs off millis() at 2 Hz, redrawing only the sign (not the band) at each flip.

- [ ] **Step 1:** If you add the pure helper, write its failing host tests first (bar width at start / middle / 0 m, never growing; `+N` rounding and never under +1 in an alarm; blink phase).
- [ ] **Step 2:** Implement the panel's three signs, the camera badge, the bump diamond, the `--` limit, the new band layout and the countdown bar; the alarm's colour swap and `+N`.
- [ ] **Step 3:** main.cpp: one viewport that leaves out the band and the panel while they show; the screen redraws whole when they go (as now).
- [ ] **Step 4: Run** `tools/run_tests.ps1`, `pio run -e s3`, `pio run -e ledpanel` → pass.
- [ ] **Step 5:** BACKLOG bench gates: the signs read at arm's length in daylight; the blink is only on the sign; the battle screen's frame rate with the panel up; the panel over each screen (battle, lyrics, status).
- [ ] **Step 6: Commit** `feat(black): camera alerts show the road sign`.

