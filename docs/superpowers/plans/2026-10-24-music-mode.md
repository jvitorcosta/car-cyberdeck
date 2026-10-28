# Music mode for the car lights: implementation plan

> **Status:** superseded by song colours (#20, #23): the lights take each song's colour from the
> black deck; no microphone or beat pulse was built. See `docs/yellow-deck.md`.

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The car's inside LED light follows the music heard by the black deck's microphone (beat pulses, loudness, colour moving through
the song's palette), the outside light holds the song's main colour, and the palette comes from the album cover, the genre and the time of day.

**Architecture:** The black deck (S3) gets one shared full-duplex I2S owner, a mic analyser and a palette builder (pure, host-tested), and a
music service that streams features and the palette over UDP to the yellow deck while it receives keep-alives. The yellow deck (CYD) gets a
pure light-behaviour engine and drives the inside/outside controllers with exact RGB at a paced rate. Phase 1 (Tasks 1-4) ends at a
**hard checkpoint**: the owner drives with a readout of what the mic hears, before Phase 2 (Tasks 5-8) builds the lights.

**Tech Stack:** ESP32-S3 + ESP32 (CYD), PlatformIO (`s3`, `ledpanel`, `ledpanel_ota`), Arduino core (legacy `driver/i2s.h`, WiFiUDP,
WebServer, HTTPClient), NimBLE 2, TFT_eSPI + VT323 faces, Unity host tests (`tools\run_tests.ps1`), Connect IQ 9.3 (watch).

**Spec:** `docs/superpowers/specs/2026-10-24-music-mode-design.md` (incl. "Settled after the builder check").

## Global Constraints

- Worktree `D:\dev\projects\spotify-pokemon-deck-music`, branch `feat/music-mode` (on main c830fdd). Conventional commits, Co-Authored-By trailer. Never push.
- Public repo: no tokens; `src/config.h` / `Secrets.mc` / garmin `bin/` never committed. English voice only.
- `-Werror` (`-Wshadow`, `-Wdouble-promotion`, `-Wformat-truncation`); pure logic in `src/util/` with host tests (`test/test_<name>/`).
- Black deck: no new TLS, no large allocations while music mode runs; audio buffers static; the music box stops while music mode listens.
- Mic settings (proven): I2S 16 kHz, MCLK ×384, ES8311 regs 0x14=0x1A, 0x15=0x40, 0x16=gain 7 (+42 dB), 0x17=0xEF (+24 dB), 0x1B=0x0A, 0x1C=0x6A; mic on the **left** channel.
- Outside light never pulses (law + owner): steady palette colour, crossfade ≥ 5 s on a new song only.
- Inside writes ≤ 15 frames/s per controller in total; no brightness step faster than 4 Hz at full depth.
- Music mode never switches on a light left off (outside); any manual change from any source ends it.
- INSIDE = `LED_INSIDE_MAC`, OUTSIDE = `LED_OUTSIDE_MAC` (confirmed by the owner).
- Builds that must pass after every task that touches them: `pio run -e s3`, `pio run -e ledpanel`; host suite all green.
- Deliver the yellow deck by WiFi when testing (`pio run -e ledpanel_ota -t upload`, PC on the hotspot), the black deck by `s3_ota` or USB.

## Review Focus

1. **Silence or hum only** (car idle, music off): no beats, `present` false, inside light settles steady — never flickers. *Pinned in Tasks 1 and 5.*
2. **Very loud or clipped music**: energy saturates without wrapping, beats still ≤ 4/s, pulses never exceed the cap. *Tasks 1 and 5.*
3. **The black deck goes away mid-song** (out of range, rebooted): within ~3 s the inside light settles on the last palette colour, no stale pulsing; the mode stays on and resumes. *Task 5 (timeout) and Task 6.*
4. **A cover with no colour** (black-and-white art, no art yet, art fetch failed): the genre family's colours; genre unknown too → a neutral default palette. *Task 2.*
5. **A manual change during music mode** (tap, preset, voice, watch, Xbox Y/LB/RB): music mode ends and that exact change applies; the outside light isn't left at the music colour. *Task 6.*

---

## File structure

| File | Responsibility |
|---|---|
| `src/util/musicfeat.h/.cpp` (new) | mic analysis: band filters, adaptive floor, beat (bass flux), energy, brightness, presence |
| `src/util/songpalette.h/.cpp` (new) | palette from an RGB565-BE cover + genre badge + minute of day |
| `src/util/musicwire.h/.cpp` (new) | UDP packet encode/decode (features, palette) |
| `src/board/s3/audioport.h/.cpp` (new) | the S3's one I2S owner (TX+RX, ref-counted); `audiosink` and `mic` use it |
| `src/board/s3/mic_es8311.h/.cpp` (new, from `feat/voice-agent`) | mic read on top of audioport |
| `src/audio/musicsvc.h/.cpp` (new, S3) | keep-alive route, capture task, UDP stream, palette per song, debug readout |
| `src/util/musiclights.h/.cpp` (new) | yellow-deck light engine: features + palette + time → inside/outside RGB/level, pacing |
| `src/util/ledvoice.h`, `src/util/lightstatus.*`, `src/util/deckmenu.*`, `src/ui/termlayout.h` (modify) | "music" word; `/status` music; a third dashboard button |
| `src/ledpanel.cpp` (modify) | mode, UDP receiver, keep-alive, exact-RGB writes, UI |
| `garmin/carlights/source/*` (modify) | MUSIC item |
| docs (`docs/leds-voice.md`, `docs/BACKLOG.md`, `docs/ARCHITECTURE.md` if it lists S3 routes) | how to use it, checks |

---

## Phase 1: the black deck listens

### Task 1: Mic analyser (pure)

**Files:** Create `src/util/musicfeat.h`, `src/util/musicfeat.cpp`; Test `test/test_musicfeat/test_musicfeat.cpp`

**Interfaces — Produces:**
```cpp
namespace musicfeat {
constexpr int RATE = 16000;
struct Features { bool beat; uint8_t strength, energy, brightness; bool present; };
class Analyzer {
public:
    // Feed mono 16 kHz samples (any block size); nowMs advances with the samples' end time.
    void feed(const int16_t* s, size_t n, uint32_t nowMs);
    // The features since the last take(): beat = at least one beat happened (strength = the strongest, 0..255);
    // energy / brightness / present as of now.
    Features take();
};
}
```
Values (constants in the header, tunable later): bass band-pass 70-180 Hz, mid 300-2000 Hz, treble high-pass 3000 Hz (biquads, float);
analysis hop 32 ms (512 samples); noise floor per band = slow follower of the band power's lower envelope (rise τ 30 s, fall τ 2 s, held
while beats are coming); beat = bass flux > 1.6 × median(flux, last 1 s) and bass power > 2 × floor, refractory 250 ms; energy = smoothed
(attack 50 ms, release 400 ms) total power above floor, normalised by a rolling peak (decay τ 10 s) → 0..255; brightness = smoothed
treble/(bass+mid) → 0..255; present = energy > 25 for ≥ 1 s within the last 3 s.

- [ ] **Step 1: Write the failing tests** (synthetic signals generated in the test, 16 kHz):
  - `test_hum_only_is_silent`: 10 s of 60 Hz sine RMS 600 + white noise RMS 150 → no `beat` in any take(), `present` false after 5 s.
  - `test_kicks_over_hum_beat_at_tempo`: hum as above + a 100 Hz burst (RMS 2000, 60 ms, decaying) every 500 ms for 10 s → beats counted in seconds 3-10 within 14..18 (≈2/s); `present` true.
  - `test_refractory`: bursts every 100 ms → ≤ 4 beats per second.
  - `test_energy_normalises`: kicks at RMS 2000 and at RMS 8000 both reach energy ≥ 150 after 3 s; clipped input (all ±32767) keeps energy ≤ 255, no wrap.
  - `test_brightness_orders`: a 5 kHz tone gives brightness higher than a 150 Hz tone of the same RMS.
- [ ] **Step 2: Run** `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1` → test_musicfeat FAILs.
- [ ] **Step 3: Implement** in `musicfeat.cpp` (RBJ biquads, per-hop power, the rules above; no heap).
- [ ] **Step 4: Run** the suite → all pass. `pio run -e s3` and `pio run -e ledpanel` still build (util/ is compiled into both).
- [ ] **Step 5: Commit** `feat(music): mic analyser, beats and energy over an adaptive floor`.

### Task 2: Song palette (pure)

**Files:** Create `src/util/songpalette.h/.cpp`; Test `test/test_songpalette/test_songpalette.cpp`

**Interfaces — Produces:**
```cpp
namespace songpalette {
struct Rgb { uint8_t r, g, b; };
struct Palette { Rgb c[3]; uint8_t cap; uint8_t depth; };   // cap: max brightness 0..100, depth: pulse depth 0..100
// Up to 3 dominant colours of an RGB565 big-endian bitmap (the S3's art buffer), skipping pixels with
// saturation < 25 % or value < 15 % or > 95 %; hues bucketed by 15°, picked ≥ 30° apart, returned at full S and V. Returns the count.
int fromCover(const uint16_t* bmpBe, int w, int h, Rgb out[3]);
// The genre family's 3 colours for a genrebadge index (0..30, genre::NONE = 0xFF → neutral).
void family(uint8_t badge, Rgb out[3]);
// Cover + genre + time: < 2 cover colours → the family; otherwise each cover hue nudged ≤ 15° toward the family's nearest hue.
// minuteOfDay < 0 (clock not synced) = day. Day (06:00-18:00): cap 100, depth 60. Evening/night (18:00-22:00, 05:00-06:00): warmer
// (hues 180-270° shifted 15° toward 300°, whites → amber), cap 70, depth 45. Late (22:00-05:00): same warm shift, cap 45, depth 30.
Palette build(const Rgb* cover, int coverCount, uint8_t badge, int minuteOfDay);
}
```
Families (badge indices from `src/util/genre_data.inc`): rock/metal/alt (6,7,8) red, orange, deep orange; electro/dance (9,10) blue, purple, cyan;
pop/k-pop/j-pop (0,1,2) pink, magenta, violet; rap/R&B/funk/afro (3,4,5,27) purple, gold, magenta; jazz/classic/OST/folk/country/gospel
(21,23,24,26,25,22) teal, warm white, soft amber; Brasil/funk BR/sertanejo/MPB/samba/pagode/forró/axé/latin/reggae (11-20) amber, green,
yellow; others/unknown (28,29,30, NONE) teal, purple, warm white.

- [ ] **Step 1: Failing tests:** `test_cover_red_blue` (half red, half blue 92×92 → 2 colours, red and blue); `test_grey_cover_uses_genre` (grey
  cover + badge 7 → the rock family); `test_no_genre_no_cover_neutral` (empty cover, NONE → the neutral family); `test_nudge_toward_genre`
  (an orange cover + electro → hue moved ≤ 15° toward blue/purple, never past it); `test_time_of_day` (same input at 12:00 → cap 100, depth 60;
  20:00 → cap 70 and a blue hue shifted toward purple; 23:30 → cap 45, depth 30; -1 → day).
- [ ] **Step 2: Run** → FAIL. **Step 3: Implement** (HSV helpers inside the module). **Step 4: Run** → pass; s3 + ledpanel build.
- [ ] **Step 5: Commit** `feat(music): song palette from the cover, genre and time of day`.

### Task 3: One I2S owner on the black deck + the mic

**Files:** Create `src/board/s3/audioport.h/.cpp`, `src/board/s3/mic_es8311.h/.cpp` (port from `git show feat/voice-agent:src/board/s3/mic_es8311.*`
onto audioport); Modify `src/board/s3/sink_es8311.cpp` (use audioport), `src/board/s3/es8311.*` (add the ADC register setup as `enableMic(uint8_t gain, uint8_t adcVol)`).

**Interfaces — Produces:**
```cpp
namespace audioport {   // I2S_NUM_0, MASTER|TX|RX, 16 kHz or 48 kHz, MCLK ×384, DMA 4×256, pins from pins.h incl. DIN
bool acquire(int rate);   // installs on the first user (or reinstalls if idle at another rate); false if busy at another rate
void release();           // uninstalls when the last user releases
}
namespace mic {           // left channel; only valid while acquired at 16 kHz
bool begin();             // audioport::acquire(16000) + es8311::enableMic(7, 0xEF)
size_t read(int16_t* mono, size_t maxSamples);   // blocking, ≤ 256
void end();               // audioport::release()
}
```
Rules: `audiosink::open/close` keep their signatures and behaviour (amp on/off, volume) but go through `audioport`; a clip at 48 kHz while the
mic holds 16 kHz is refused (`open` returns false → the caller already handles a failed open). `greeting`'s `s_output` mutex stays the
arbiter for playback; the mic doesn't take it.
- [ ] **Step 1: Implement** audioport + mic + sink rewiring. Device-only (no host test); keep pure bits (if any) out.
- [ ] **Step 2: Build** `pio run -e s3` (-Werror) and run the host suite.
- [ ] **Step 3: On the black deck** (checklist in the commit body): the greeting plays at boot; the phone remote's `/play?clip=howl` still plays;
  the music box on the waiting screen still plays.
- [ ] **Step 4: Commit** `refactor(s3): one I2S owner for the speaker and the mic`.

### Task 4: Music service on the black deck + wire format + readout

**Files:** Create `src/util/musicwire.h/.cpp` + `test/test_musicwire/`, `src/audio/musicsvc.h/.cpp`; Modify `src/net/remote.cpp` (routes),
`src/main.cpp` / `src/core/nettask.cpp` (palette hook after art decode, genre), `docs/ARCHITECTURE.md` if it lists routes.

**Interfaces — Produces:**
```cpp
namespace musicwire {
constexpr uint16_t PORT = 4210;
struct FeatPkt { uint16_t seq; musicfeat::Features f; };
struct PalPkt  { uint32_t songId; songpalette::Palette p; };
size_t encode(const FeatPkt&, uint8_t* out, size_t n);   // 'M','F',1, then fields; 9 bytes
size_t encode(const PalPkt&, uint8_t* out, size_t n);    // 'M','P',1, then fields; 18 bytes
bool decode(const uint8_t* in, size_t n, FeatPkt* f);    // false on wrong magic/version/length
bool decode(const uint8_t* in, size_t n, PalPkt* p);
}
```
S3 behaviour:
- `GET /music?k=…&on=1|0` (key-gated like `/state`): on=1 starts or keeps music mode alive, recording the requester's IP (`s_server->client().remoteIP()`);
  on=0 stops at once. No keep-alive for 30 s → stops. Starting stops the music box (`greeting::stopTunes`) and keeps it off while on.
- A capture task (core 1, prio 1, ~4 KB stack, static 256-sample buffer): `mic::begin()`, read → `Analyzer::feed`, every ~33 ms `take()` →
  `FeatPkt` → UDP to the requester:4210. `mic::end()` on stop.
- Palette: after a new song's art decodes (nettask's `Step::Art` ok branch) → `songpalette::fromCover(art::bitmap(), 92, 92)`; genre badge from
  the track's genre (keep the current badge readable from the network task — a small `shared::genreNow()`); minute from `netclock`.
  `build()` → `PalPkt{songId = trackGen}`; sent on change and every 10 s while on. No art yet → cover count 0.
- `GET /music/debug?k=…` → `{"on":1,"floor":[b,m,t],"energy":e,"bright":x,"present":p,"beatsPerMin":n,"palette":[[r,g,b],…],"cap":c}`.
  Serial: `[music] floor b/m/t energy e beats/min n present p` every 2 s while on.
- [ ] **Step 1: Failing tests** for musicwire: round trips of both packets; wrong magic, version 2, short length → false.
- [ ] **Step 2: Run** → FAIL. **Step 3: Implement** musicwire, then musicsvc + routes + hooks.
- [ ] **Step 4: Run** the suite; `pio run -e s3` builds.
- [ ] **Step 5: On the black deck at home** (from a PC on the same network): `curl ".../music?k=<REMOTE_TOKEN>&on=1"` then `.../music/debug` shows
  the floor and energy moving when you clap/play music; after 30 s without keep-alive `on` returns to 0 and the music box may play again.
- [ ] **Step 6: Commit** `feat(music): the black deck streams what its mic hears and the song's palette`.

### ⛔ Checkpoint (owner): drive with the readout

Stop after Task 4. The owner drives with music at the usual volume (~18) while `/music/debug` (or the serial log) is captured for a few songs.
**Go** for Phase 2 if `present` is true most of the time music plays and beats/min lands near the songs' tempo; **no-go** → Phase 2 builds only
the steady "song-colour" fallback (Task 5's no-signal path, Task 6 without beat handling), as the spec allows.

---

## Phase 2: the yellow deck drives the lights

### Task 5: Light engine (pure)

**Files:** Create `src/util/musiclights.h/.cpp`; Test `test/test_musiclights/test_musiclights.cpp`

**Interfaces — Consumes:** `musicfeat::Features`, `songpalette::Palette/Rgb`. **Produces:**
```cpp
namespace musiclights {
struct Out { songpalette::Rgb rgb; uint8_t level; bool send; };   // level 0..100; send = time to write this light
class Engine {
public:
    void onPalette(const songpalette::Palette& p, uint32_t songId, uint32_t nowMs);
    void onFeatures(const musicfeat::Features& f, uint32_t nowMs);
    // Called every loop pass; at most MAX_HZ sends per light (inside 7 Hz: two ELK variants per update ≤ 15 frames/s; outside 1 Hz).
    Out inside(uint32_t nowMs);
    Out outside(uint32_t nowMs);
};
int nearestSwatch(songpalette::Rgb c);   // ledlook swatch index 0..7 for the theme
}
```
Rules (spec "Light behaviour"): base colour picked by the 8 s mean energy across the palette (calm → the palette colour with the lowest V×S, loud →
the highest); big change (mean energy ×2 within 2 s after ≥ 4 s calm) → next palette colour on that beat; otherwise crossfade over 2 s; beat →
level jumps by `depth` and decays to base over 200 ms, never above `cap`; base level = cap × (0.35 + 0.65 × energy/255); no features for 3 s or
`present` false → settle in 3 s on c[0] at cap × 0.5, steady. Outside: c[0] at cap × 0.7, crossfade 5 s on a new songId, no beat or energy effect.
- [ ] **Step 1: Failing tests:** `test_quiet_is_steady` (present false for 10 s → inside level constant ±1 and rgb == c[0] after 3 s);
  `test_beat_pulses_within_cap` (beats at 2/s → level rises then decays within 200 ms, never > cap); `test_rate_limit` (1000 calls in 1 s →
  inside `send` ≤ 7 times, outside ≤ 1); `test_outside_never_pulses` (beats every 300 ms → outside level constant, rgb only changes over ≥ 5 s
  after a new song); `test_lost_link_settles` (features stop → settles on c[0] within 3 s, no pulses); `test_big_change_moves_colour`;
  `test_nearest_swatch` (pure red → 0, pure blue → 4, warm white → 6).
- [ ] **Step 2: Run** → FAIL. **Step 3: Implement.** **Step 4: Run** → pass; ledpanel + s3 build.
- [ ] **Step 5: Commit** `feat(music): the light engine, pulses inside and a steady outside`.

### Task 6: Music mode on the yellow deck

**Files:** Modify `src/ledpanel.cpp`, `src/util/ledvoice.h` + test, `src/util/lightstatus.*` + test, `src/util/deckmenu.*` + test,
`src/ui/termlayout.h` + test_termfont.

Behaviour:
- **Control: a third dashboard button `[MUSIC]`** (`deckmenu` items row: `{4,192,100,44}`, `{110,192,100,44}`, `{216,192,100,44}` for LIGHTS,
  KARAOKE, MUSIC; ≥ 70×36). *Plan decision:* the dashboard, not the Lights page (one tap from boot, the Lights page has no free finger-sized
  slot); lit with a `LIVE`-style `ON` tag while the mode runs. Tap toggles on/off.
- **Voice:** `ledvoice` word `music` → `Command::music = 1` (a colour/preset/power/level in the same phrase wins and means off). Reply
  `"Music mode on"`. `/status` appends ` music=on|off` (`lightstatus::reply` gains a bool).
- **Mode:** on → the inside light on (if off), the outside light kept off if it was off; a keep-alive `GET /music?on=1` to the black deck every
  10 s via the lyrics task's host logic (`lyricsHost`); off → `on=0`, the looks restored to what they were before music mode (saved on entry).
  Any manual change (touch colour/preset/level/zone, Xbox A/Y/LB/RB, voice colour/preset/off/level, watch) → music off, then that change.
- **UDP receiver task** (core 1, prio 0, ~4 KB, `WiFiUDP` on `musicwire::PORT`): decodes packets → a queue → loop() feeds the Engine. Drops packets
  not from the black deck's address.
- **Writes:** the loop sends Engine outputs with exact RGB: `elkcmd::color(v, dim(r,level), …)` both variants per update for the inside light at
  ≤ 7 updates/s, the outside at ≤ 1/s; never while a light is unlinked.
- **Dashboard:** the light rows show `MUSIC` with the live chip; messages `>MUSIC ON` / `>MUSIC OFF` / `>NO SIGNAL` (not ALERT, after 3 s
  without features). **Theme** = `nearestSwatch(palette.c[0])` while on (changes per song only).
- Fix the INSIDE/OUTSIDE "guess" comment (confirmed) and close the BACKLOG item.
- [ ] **Step 1: Failing tests:** ledvoice `test_music_word` (`"music"` → music 1; `"music blue"` → colour 4, music -1); lightstatus
  `test_reply_music` (`"inside=on outside=off music=on"`); deckmenu three-button rects/neighbours; termfont fit for `> [MUSIC]` + tag.
- [ ] **Step 2: Run** → FAIL. **Step 3: Implement** the pure parts, then ledpanel.
- [ ] **Step 4: Run** the suite; `pio run -e ledpanel` builds.
- [ ] **Step 5: On the deck** (via `ledpanel_ota`): MUSIC toggles, the keep-alive reaches the black deck (`/music/debug` on=1), the inside light
  follows a song at home, a colour tap ends the mode, `>NO SIGNAL` with the black deck off.
- [ ] **Step 6: Commit** `feat(leds): music mode on the yellow deck`.

### Task 7: Watch MUSIC item

**Files:** Modify `garmin/carlights/source/ZonesDelegate.mc` (PRESETS menu: a first item `MUSIC` → `/say?text=music`, not absorbed, then syncLooks),
`Deck.mc` if needed. Build with the monkeyc command (no new warnings); sideload over MTP; check it toggles music mode.
- [ ] **Commit** `feat(garmin): MUSIC on the presets list`.

### Task 8: Docs

`docs/leds-voice.md` (music mode: how to switch, what the lights do, outside steady, `>NO SIGNAL`, voice word, `/status` music, `/music` and
`/music/debug` on the black deck), `docs/BACKLOG.md` (car checks: beats at volume ~18, night caps, no outside pulsing, manual change exits).
- [ ] **Commit** `docs(leds): music mode`.
