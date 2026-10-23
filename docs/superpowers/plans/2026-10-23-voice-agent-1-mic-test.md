# Voice agent, part 1: car mic test — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Record real Portuguese commands from the black deck's own mic in the car, and score them
through candidate speech-to-text services, so the owner can decide go / new mic before anything
else is built.

**Architecture:** A standalone recorder firmware (`env:micrec_s3`, its own entry like
`hwcheck_s3.cpp`) shows the next phrase on screen, records while the BOOT button is held into PSRAM,
and serves the clips as WAV over the hotspot. A Python tool pulls them to the PC; a second tool
sends each clip to the STT services and scores it. A short results doc records the owner's call.

**Tech Stack:** Arduino-ESP32 on PlatformIO (`esp32-s3-devkitc-1`, TFT_eSPI, `WebServer`, legacy
`driver/i2s.h` as in the spike), host tests with Unity via `tools/run_tests.ps1`, Python 3 stdlib
(`unittest`, `urllib`, `wave`) for the tools.

**Spec:** `docs/superpowers/specs/2026-10-23-voice-agent-design.md` ("First step: prove the mic in
the car").

**This is part 1 of 3.** Part 2 (the cloud agent: fast path, tools, Agent SDK slow path, session
state, tracing, text evals) and part 3 (deck integration: `/talk`, audio streaming, replies, stock
phrases, Zero Two on the S3, RT/LT on the yellow deck) get their own plans. Part 3 only starts if
this test passes; part 2 doesn't depend on the mic and may start once the results are in.

## Global Constraints

- v1 is **Portuguese only**; English artist names inside a Portuguese sentence count.
- Mic: ES8311 at **16 kHz, 16-bit mono**, settings from the spike: reg `0x14=0x1A`, `0x15=0x40`,
  `0x16` mic gain, `0x17` ADC volume, `0x1B=0x0A`, `0x1C=0x6A`; I2S full duplex, MCLK ×384,
  pins from `include/pins.h` (`PIN_I2S_*`, `PIN_I2C_*`).
- **Pass bar:** at least **9 of 10** commands actionable in **each** of the 4 conditions, with the
  best STT provider. The owner sees the results and makes the call.
- Recordings are the owner's voice: **git-ignored** (`micrec/`), never committed; WAV endpoints
  need `REMOTE_TOKEN` (`k=`) like the existing remote.
- STT API keys come only from environment variables (`DEEPGRAM_API_KEY`, `OPENAI_API_KEY`,
  `ELEVENLABS_API_KEY`); a provider without a key is skipped, not an error.
- No SD card (it starves the TLS heap); clips live in PSRAM only, so they're lost at power-off:
  pull them before turning the car off.
- Commits: conventional (`type(scope): subject`), ending with the Co-Authored-By line.

## Review Focus

- **BOOT held past 4 s:** recording stops at 4 s and the clip is kept (not dropped, not overrun).
- **A tap on BOOT (< 0.3 s):** ignored, no empty clip, the phrase doesn't advance.
- **All 40 clips taken:** further presses do nothing and the screen says "cheio — baixe os clips".
- **Re-recording a phrase** (tap the screen's left half): the old clip for that slot is replaced,
  not appended, so the manifest keeps exactly one clip per (condition, phrase).
- **Accents and punctuation in transcripts** ("Põe", "música.", "Tyler, The Creator"): scoring
  normalises them, so a correct transcript isn't failed on spelling.

---

## The test set

Ten phrases (`P1`–`P10`), each with the keywords a transcript must contain to be **actionable**
(after normalisation: lower case, accents removed, punctuation dropped):

| # | Say | Keywords |
|---|---|---|
| P1 | liga as luzes | `liga`, `luz` |
| P2 | deixa azul fraquinho dentro | `azul`, `fraquinho`, `dentro` |
| P3 | apaga a luz de fora | `apaga`, `fora` |
| P4 | roxo cinquenta por cento | `roxo`, `50` |
| P5 | toca Djavan | `toca`, `djavan` |
| P6 | pula essa música | `pula` |
| P7 | volta a música anterior | `volta`, `anterior` |
| P8 | toca Tyler, the Creator | `toca`, `tyler` |
| P9 | toca Billie Eilish | `toca`, `billie` |
| P10 | aumenta o volume | `aumenta`, `volume` |

`50` also matches `cinquenta`. A keyword matches a whole word, or a word that starts with it
(`luz` matches `luzes`).

Four conditions, recorded in this order (`A`–`D`), ten clips each:

| | Spotify | Windows | Car |
|---|---|---|---|
| A | paused | closed | moving |
| B | playing, phone volume ~20 % | closed | moving |
| C | paused | open | moving |
| D | playing, phone volume ~20 % | open | moving |

Clip slot `i` (0–39) = condition `i / 10`, phrase `i % 10`.

---

### Task 1: Pure recorder logic (WAV header, slots, phrases)

**Files:**
- Create: `src/util/micrec.h`, `src/util/micrec.cpp`
- Test: `test/test_micrec/test_micrec.cpp`

**Interfaces:**
- Produces:
  - `namespace micrec { constexpr int RATE = 16000; constexpr int MAX_MS = 4000; constexpr int MIN_MS = 300; constexpr int SLOTS = 40; constexpr int PHRASES = 10; }`
  - `const char* micrec::phrase(int p)` — the "Say" text of P1–P10 (`p` 0–9), `""` out of range.
  - `char micrec::condition(int slot)` — `'A'`–`'D'`, `'?'` out of range.
  - `int micrec::phraseOf(int slot)` — `slot % 10`, `-1` out of range.
  - `void micrec::wavHeader(uint8_t out[44], uint32_t dataBytes, uint32_t rate)` — canonical PCM
    RIFF header, mono, 16-bit.
  - `int micrec::samplesToKeep(uint32_t heldMs)` — `0` below `MIN_MS`, else
    `min(heldMs, MAX_MS) * RATE / 1000`.
  - `void micrec::clipName(int slot, char* out, size_t n)` — `"A01"`…`"D10"`.

- [ ] **Step 1: Write the failing tests**

```cpp
void test_slots_map_to_condition_and_phrase() {
    TEST_ASSERT_EQUAL_CHAR('A', micrec::condition(0));
    TEST_ASSERT_EQUAL_CHAR('D', micrec::condition(39));
    TEST_ASSERT_EQUAL_CHAR('?', micrec::condition(40));
    TEST_ASSERT_EQUAL_INT(9, micrec::phraseOf(19));
    TEST_ASSERT_EQUAL_STRING("toca Djavan", micrec::phrase(4));
}
void test_wav_header_is_16k_mono_pcm() {
    uint8_t h[44];
    micrec::wavHeader(h, 128000, 16000);
    TEST_ASSERT_EQUAL_MEMORY("RIFF", h, 4);
    TEST_ASSERT_EQUAL_UINT32(36 + 128000, h[4] | h[5] << 8 | h[6] << 16 | (uint32_t)h[7] << 24);
    TEST_ASSERT_EQUAL_MEMORY("WAVEfmt ", h + 8, 8);
    TEST_ASSERT_EQUAL_UINT16(1, h[20] | h[21] << 8);          // PCM
    TEST_ASSERT_EQUAL_UINT16(1, h[22] | h[23] << 8);          // mono
    TEST_ASSERT_EQUAL_UINT32(16000, h[24] | h[25] << 8 | h[26] << 16);
    TEST_ASSERT_EQUAL_UINT16(16, h[34] | h[35] << 8);
    TEST_ASSERT_EQUAL_MEMORY("data", h + 36, 4);
}
void test_short_taps_are_dropped_and_long_holds_capped() {
    TEST_ASSERT_EQUAL_INT(0, micrec::samplesToKeep(250));
    TEST_ASSERT_EQUAL_INT(16000, micrec::samplesToKeep(1000));
    TEST_ASSERT_EQUAL_INT(64000, micrec::samplesToKeep(9000));
}
void test_clip_names() {
    char s[8];
    micrec::clipName(0, s, sizeof s);  TEST_ASSERT_EQUAL_STRING("A01", s);
    micrec::clipName(39, s, sizeof s); TEST_ASSERT_EQUAL_STRING("D10", s);
}
```

- [ ] **Step 2: Run to verify they fail**

Run: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1`
Expected: `test_micrec` FAILS (missing header); the other suites pass.

- [ ] **Step 3: Implement `src/util/micrec.{h,cpp}`** (pure C++, no Arduino includes, like
  `util/hotspotip`). Phrase texts exactly as in the test-set table.

- [ ] **Step 4: Run tests, all pass**

- [ ] **Step 5: Commit** — `feat(micrec): pure clip, phrase and WAV header logic`

---

### Task 2: ES8311 mic input

**Files:**
- Create: `src/board/s3/mic_es8311.h`, `src/board/s3/mic_es8311.cpp`

**Interfaces:**
- Produces:
  - `bool mic::begin(int rate)` — installs I2S full duplex on `I2S_NUM_0`, MCLK ×384, starts the
    codec (`es8311::begin`), sets the ADC registers from Global Constraints with mic gain
    `0x16=0x07` (+42 dB) and ADC volume `0x17=0xBF` (0 dB: the spike's +24 dB put claps at ~21k
    of 32k, too little headroom for a voice right at the deck; STT prefers clean over loud). Logs `[mic] codec up, adc on` or what failed.
  - `size_t mic::read(int16_t* mono, size_t maxSamples)` — blocks until samples arrive, returns
    the left channel only.
  - `void mic::setGain(uint8_t reg16)` — rewrites `0x16` (for the gain check in Task 3).

- [ ] **Step 1: Implement from `spike/mictest-s3:src/mictest_s3.cpp`** (I2S config and register
  writes), split into the functions above. No host test: it's register writes on hardware;
  Task 3 verifies it.
- [ ] **Step 2: Build** — `pio run -e s3` still builds (the module is compiled but unused there).
- [ ] **Step 3: Commit** — `feat(s3): ES8311 mic input module`

---

### Task 3: Recorder firmware

**Files:**
- Create: `src/micrec_s3.cpp`
- Modify: `platformio.ini` (new `[env:micrec_s3]`; add `-<micrec_s3.cpp>` to `env:s3`'s
  `build_src_filter`), `.gitignore` (add `micrec/`)

**Interfaces:**
- Consumes: Task 1 (`micrec::*`), Task 2 (`mic::*`), `net::connectAny()`, `net::deviceIp()`,
  `REMOTE_TOKEN` from `config.h`.
- Produces (HTTP on port 80, every route needs `k=<REMOTE_TOKEN>`, else 403):
  - `GET /` — a phone page: one row per recorded slot (name, phrase, seconds) with a WAV link.
  - `GET /clip?i=<slot>` — `audio/wav`, header from `micrec::wavHeader`; 404 for an empty slot.
  - `GET /manifest.json` — `[{"slot":0,"name":"A01","condition":"A","phrase":0,"ms":1830,"gain":7}, …]`
    for recorded slots only.
  - `POST /gain?g=<0..7>` — `mic::setGain`, kept for later clips (recorded in each clip's
    `"gain"`), so the owner can try a lower gain if A clips clip.

Behaviour (the screen is 320×240 landscape, TFT_eSPI, the font sizes the boot terminal uses):
- Top line: `Condição A — Spotify pausado, vidros fechados` (B–D from the conditions table).
- Middle, large: the current phrase. Under it: `3/10`, and the deck's address + `/?k=…` once online.
- **BOOT (GPIO 0) held:** red `● GRAVANDO` and a level bar; records into a PSRAM buffer
  (`ps_malloc`, `SLOTS × MAX_MS` of 16-bit samples, 5.1 MB, allocated once at boot). On release,
  `samplesToKeep` decides: 0 → nothing happens; otherwise the slot is (re)written and the phrase
  advances.
- **Screen tap, left half:** back one slot (to redo it). **Right half:** skip forward.
- After slot 39: `cheio — baixe os clips` and BOOT does nothing.
- Serial log per clip: `[micrec] A03 1.8 s peak 18234 rms 2210`.

- [ ] **Step 1: Write `src/micrec_s3.cpp` and the env** (`extends = env:s3`,
  `build_src_filter = -<*> +<micrec_s3.cpp> +<board/s3/> +<util/micrec.cpp> +<net/wifi.cpp>` plus
  whatever `net/wifi.cpp` pulls in; `board_build.embed_files =` empty like the spike).
- [ ] **Step 2: Build** — `pio run -e micrec_s3` succeeds; `pio run -e s3` still succeeds.
- [ ] **Step 3: Flash and check on the bench** — `pio run -e micrec_s3 -t upload --upload-port COM12`.
  Expected: boot log shows `[mic] codec up, adc on` and the PSRAM buffer allocated; holding BOOT
  and saying P1 logs a clip with peak above ~5000; a 0.2 s tap logs nothing; the phone page on the
  same network lists the clip and plays it back intelligibly.
- [ ] **Step 4: Commit** — `feat(micrec): car mic recorder firmware for the voice agent test`

---

### Task 4: Pull tool

**Files:**
- Create: `tools/micrec_pull.py`, `tools/test_micrec_pull.py`

**Interfaces:**
- Consumes: Task 3's `/manifest.json` and `/clip`.
- Produces: `python tools/micrec_pull.py <host> <token> [--out micrec/<YYYY-MM-DD>]` writes
  `<name>.wav` per clip plus `manifest.json` (the deck's manifest, with `"file"` added).
  `pull(base_url, token, out_dir) -> list[dict]` is the importable entry point.

- [ ] **Step 1: Write the failing test** — `PullFromFakeDeck.test_writes_wavs_and_manifest`: a
  `http.server` thread serving a two-clip manifest and two small WAVs; after `pull(...)`, the out
  dir holds `A01.wav`, `A02.wav` (bytes equal to what was served) and a manifest whose entries have
  `"file": "A01.wav"`. `test_wrong_token_raises`: the fake answers 403 → `pull` raises with a
  message containing `403`.
- [ ] **Step 2: Run** `python tools/test_micrec_pull.py` → fails (no module).
- [ ] **Step 3: Implement with `urllib.request`** (stdlib only).
- [ ] **Step 4: Run** → `OK`.
- [ ] **Step 5: Commit** — `feat(micrec): pull the car clips to the PC`

---

### Task 5: STT bench

**Files:**
- Create: `tools/stt_bench.py`, `tools/test_stt_bench.py`

**Interfaces:**
- Consumes: a folder written by Task 4.
- Produces: `python tools/stt_bench.py micrec/<date>` prints, and writes to
  `micrec/<date>/report.md`, one table per provider: rows A–D, columns
  `actionable n/10`, `pass (≥9)`, `median latency ms`; then each failed clip with its transcript.
  Importable pieces:
  - `normalise(text: str) -> list[str]` — lower case, accents stripped (`unicodedata` NFKD),
    punctuation removed, split on spaces; `cinquenta` → `50`.
  - `actionable(transcript: str, keywords: list[str]) -> bool` — every keyword equals or prefixes
    some word of `normalise(transcript)`.
  - `KEYWORDS: list[list[str]]` — the test-set table, P1–P10.
  - `score(results: list[dict]) -> dict[str, dict]` — input rows
    `{"condition": "A", "phrase": 0, "transcript": "...", "ms": 412}`; output per condition
    `{"ok": 9, "n": 10, "pass": True, "median_ms": 410}`.
  - Providers, each `transcribe(wav: bytes) -> str`, timed by the caller, language `pt` / `pt-BR`:
    Deepgram (prerecorded REST, `nova-3`, `language=pt-BR`, `keyterm` for `Djavan`, `Tyler`,
    `Billie Eilish`), OpenAI (`/v1/audio/transcriptions`, `gpt-4o-transcribe`, `language=pt`,
    the same names in `prompt`), ElevenLabs Scribe (`/v1/speech-to-text`, `scribe_v1`,
    `language_code=por`). Skipped with a note when its key isn't set.

- [ ] **Step 1: Write the failing tests**

```python
class Scoring(unittest.TestCase):
    def test_accents_case_and_punctuation_dont_matter(self):
        self.assertTrue(sb.actionable("Põe... TOCA Djavan!", ["toca", "djavan"]))
    def test_prefix_and_number_words(self):
        self.assertTrue(sb.actionable("liga as luzes", ["liga", "luz"]))
        self.assertTrue(sb.actionable("Roxo, cinquenta por cento.", ["roxo", "50"]))
        self.assertTrue(sb.actionable("roxo 50%", ["roxo", "50"]))
    def test_missing_keyword_fails(self):
        self.assertFalse(sb.actionable("toca já vá", ["toca", "djavan"]))
    def test_pass_bar_is_nine_of_ten_per_condition(self):
        rows = [{"condition": "A", "phrase": p, "transcript": "x", "ms": 100} for p in range(10)]
        for r in rows[:9]:
            r["transcript"] = " ".join(sb.KEYWORDS[r["phrase"]])
        s = sb.score(rows)
        self.assertEqual((s["A"]["ok"], s["A"]["pass"]), (9, True))
        rows[8]["transcript"] = "x"
        self.assertFalse(sb.score(rows)["A"]["pass"])
```

- [ ] **Step 2: Run** `python tools/test_stt_bench.py` → fails.
- [ ] **Step 3: Implement** (stdlib `urllib` for the three REST calls, multipart by hand for
  OpenAI and ElevenLabs).
- [ ] **Step 4: Run** → `OK`. Then a smoke run on the bench clip from Task 3 with at least one key
  set: the report shows a transcript for it.
- [ ] **Step 5: Commit** — `feat(micrec): STT bench that scores the car clips`

---

### Task 6: The car session and the decision (owner + agent)

**Files:**
- Create: `docs/voice-agent-mictest.md` (results only: tables and the decision, no audio, no
  transcripts with personal content beyond the ten phrases)

- [ ] **Step 1: Owner records in the car** with the recorder flashed: conditions A–D in order, ten
  phrases each: it starts by itself 5 s after power-on and runs all 40 hands-free (3-2-1, beep, 4 s each) with a 20 s break before B, C and D to change music and windows; power it on only when it's safe to talk (a screen tap pauses). Spotify volume for
  B/D set by hand on the phone.
- [ ] **Step 2: Pull before turning the car off** — PC or laptop on the hotspot:
  `python tools/micrec_pull.py 172.20.10.10 <REMOTE_TOKEN>` (or `cardeck.local`). If no PC is in
  the car, the phone page's WAV links save the clips as a fallback.
- [ ] **Step 3: Speaker check, same drive** — reflash `pio run -e s3 -t upload` (or keep it for
  the next drive), pause Spotify, and trigger the howl from the remote page
  (`POST /play?clip=howl`) while moving with windows closed, then open. Note: heard clearly / barely
  / not at all.
- [ ] **Step 4: Bench** — `python tools/stt_bench.py micrec/<date>`; copy the tables into
  `docs/voice-agent-mictest.md` with the speaker result.
- [ ] **Step 5: Owner decides** — pass (≥ 9/10 in every condition with the best provider) → part 2
  and 3 go ahead with that provider; fail → write which conditions failed and stop for the mic
  decision. Speaker too quiet → voice replies move later (screen + beep only), per the spec.
- [ ] **Step 6: Commit** — `docs(voice): car mic test results`
