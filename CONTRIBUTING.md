# Contributing to CarDeck

Thanks for your interest. Bug reports, measurements from your own board and pull requests are
all welcome. For anything bigger than a fix, open an issue first so we can agree on the
approach before you spend time on it.

## Setup

1. Follow the README's [Quick start](README.md#quick-start) to build and flash a board.
   For the serial tools (`tools/capture_serial.py`), `pip install -r tools/requirements.txt`
   (pyserial); the generators (`tools/gen_*.py`) need only the standard library, and the media
   ones ffmpeg on `PATH`.
2. Install a host compiler for the unit tests: g++ on `PATH` (on Windows, MinGW-w64; or set
   `$env:MINGW_BIN`), then Unity once:
   `pio pkg install -e native -l throwtheswitch/Unity@2.6.0`.
3. Run the tests: `powershell -ExecutionPolicy Bypass -File tools\run_tests.ps1`.
   One suite: `tools\ntest.ps1 test\<suite>\<suite>.cpp <module.cpp ...>`.
   (`pio test -e native` doesn't work with this layout.)
4. Run the media tool tests (standard library only, no ffmpeg):
   `python -m unittest discover -s tools -p "test_gen_*.py"`.

## Project layout

| Path | What lives there |
|---|---|
| `src/core/` | network task, shared state, memory accounting |
| `src/ui/` | everything drawn on the display |
| `src/util/` | pure logic, no Arduino: parsers, timing, layout; tested on the PC |
| `src/board/cyd/`, `src/board/s3/` | board-specific code; pins in `include/pins.h` |
| `test/` | one Unity suite per pure module |
| `tools/` | generators for bundled data, test runners, session analysis |
| `docs/` | architecture, history, backlog and design records |

`util/`, `core/` and `images/` must not include `ui/`, and `util/` must not include Arduino.
Code that can be pure belongs in `util/` with a test.

## Tests

Every change to logic comes with a test, written first. Firmware logic goes in a Unity suite under
`test/`; the media tools (`tools/gen_*.py`) have Python `unittest` tests next to them
(`tools/test_gen_*.py`, see Setup). CI runs both. What a test must earn:

- A test earns its place when a plausible change would make it fail **and** the failure points
  at a real problem: behaviour a user or another module relies on, a past bug, a bound, a wrap.
- Test properties, not tuning: colours, art pixels, easing midpoints and copy text change on
  purpose — assert invariants (contrast, monotonicity, endpoints, fits-in-box). Exact values only
  where they are the contract (parser output, wire formats, bit-exact encoders).
- Every parser of outside data (HTTP, XML, JSON, blobs) gets a cut-off input, an absurd number
  and an offset that would wrap on 32 bits.
- Every timer comparison gets a `millis()` wrap test; every fixed buffer a test at its limit and
  one past it.
- One table-driven test per rule, not one test per row; don't assert the same thing twice.
- No tests for constants, getters or one-line ternaries — a `static_assert` if a constant matters.

## On the device

RAM is the constraint that matters: the CYD has no PSRAM and a TLS session needs ~40 KB in one
block. For anything that allocates or adds a request, include a serial capture:

```sh
python tools/capture_serial.py <port> 600 session.log
python tools/analyze_session.py session.log
```

and mention the `[mem]` lines (free and largest block) before and after your change. Keep each
deck's app partition (`cyd`, `cyd_companion`, `s3`) under 97 % and the S3's media pack under
95 % of its partition (CI enforces both, counting your media's real sizes from
`data/media_sizes.txt`).

## Commits and pull requests

- [Conventional commits](https://www.conventionalcommits.org): `type(scope): subject`, for
  example `fix(lyrics): retry on 503` or `feat(ui): genre badge`. The type decides the next
  version number (README "Versioning").
- One logical change per commit; the tests pass at every commit.
- Never commit `src/config.h`, personal photos or audio, or anything with a number plate.
  `.gitignore` covers the usual paths; check `git status` anyway.
- Pull requests describe what changed, why, and how it was tested (host tests and which
  board).

## Code style

Match the surrounding code: 4-space indent, `camelCase` functions, `UPPER_CASE` constants,
comments that explain why rather than what. `.editorconfig` covers whitespace.
