# Boot Sound Rosters — Design

2026-11-03. Black deck (ESP32-S3, env `s3`).

## Why

The black deck plays the same three sounds at every boot. The silver Honda's night drive plays
`data/greeting.pcm`: "Bem-vindo, moreno" followed by the Japanese ETC voice, always both.
The photo montage opens with `data/opener.pcm` (an irasshaimase), and the Honda logo card ends
with `data/finale.pcm`. The owner wants each of those three moments to pick one clip at random
from a roster at every boot. The sounds he has today stay in the rosters as options. He picked
every roster by ear on a local listening page.

## Goals

- **Drive greeting** (the silver Honda's opening): one clip per boot from its roster. Today's
  pair is split into two separate entries, Bem-vindo or the ETC voice, so they no longer play
  together.
- **Montage start** and **Honda logo**: one clip each per boot from its own roster. These play
  after every opening (kiss, Honda, McQueen, Max), as they do today.
- **No repeats:** no slot plays the same clip as it did at the previous boot. Each slot's last
  pick is kept in NVS, like the opening.
- **Pure, host-tested picking logic**, in the same style as `util/intropick`.
- **Builds without the clips:** a fresh clone (no git-ignored audio) still builds and boots.
  Then each slot falls back to today's single-file behaviour.

## Non-goals

- McQueen's greeting (`greeting_alt.pcm`), the intro videos' sound and the howl stay as they are.
- No text-to-speech, and no generated or faked voices. Every clip is a real download already
  in `greeting-sounds/`.
- No change to the montage, its timing or the boot scene's visuals.

## Anti-goals

- Committing any audio. The clips stay git-ignored (several are personal-use rips), so the
  public repo only gains code, the tool and docs.
- An opener that is still playing when the finale starts. There is one I2S port, so today the
  opener is skipped when it wouldn't fit. That must stay true for every pair of picks.
- Starving the Spotify TLS heap. Clips are read from flash in place, as today. The new code
  must not add RAM buffers.

## The rosters (picked by the owner, 2026-11-03)

Sources are under `greeting-sounds/` (git-ignored; `SOURCES.txt` has links and licences).
`now_*` are today's clips exported to WAV, with `greeting.pcm` cut into its two halves.

| Slot | Clips |
|---|---|
| Drive (7 + today's 3) | now_bemvindo, now_etc, now_opener (today's irasshaimase), irasshai_1, ohayo_amitaro, ohayo_sel, engine_kidoushimashita, engine_kidou, engine_kaishi, bemvindo (clean cut) |
| Montage start (14) | chimevoice_gps, ami_itterasshai_01, ami_itterasshai_02, ami_okaerinasai_02, ami_okkaeri_01, ami_okaerinasaimattetayo_01, ami_konnichiwa_03, ami_konbanwa_03, ami_ikuyo_02, ami_ikuyo_04, ami_youistart_01, ami_hasshin_01, ami_ikimaasu_01, ami_tsukimashita_01 |
| Honda logo (12) | now_finale, drivesafe_1, drivesafe_2, chimevoice_dokohe, ami_kyouwadokoe_01, ami_readygo_01, ami_junbibantan_01, ami_ganbatte_01, ami_yoroshiku_05, ami_untennotsukare_01, ami_mousuguouchi_01, ami_otsukaresama_04 |

The longest clips are 1.84 s (drive), 2.55 s (montage start) and 3.78 s (logo). In all, about
1.3 MB of 16-bit 16 kHz mono goes into the media partition. That partition is 10.1 MB, about
7.5 MB is used today and roughly 1.3 MB of the new audio is new on top of what is already there.

## How it works

### Storage: one roster file per slot

The media pack reads at most 32 files (`tools/pack_media.py` `MAX_FILES`, which matches
`core/media.cpp`), and it holds 16 today. So each slot is one file holding all its clips, not
one file per clip:

- `data/drive.snd`, `data/opener.snd`, `data/finale.snd` (git-ignored), added to
  `pack_media.py`'s `FILES`.
- Format (little-endian): `"SNDR"`, u16 version 1, u16 count, then count × (u32 offset from the
  file's start, u32 size in bytes, u8 name length, name bytes…) is the builder's call. Then the
  clips: 16-bit signed mono 16 kHz PCM, each 2-byte aligned. Names are for the serial log only.
- A pure reader, `util/black/sndroster`, validates the header and bounds (like `mediapack` and
  `cjkfont`) and gives clip i's data and size. It is host-tested, including bad and truncated
  files.

### Building the rosters: `tools/gen_rosters.py`

- It reads `data/rosters.txt` (git-ignored, like `data/tunes.txt`): one `[drive]`, `[opener]`
  and `[finale]` section, each a list of WAV paths relative to `greeting-sounds/`.
- Each WAV goes through `gen_greeting.py`'s steps (mono, resample to 16 kHz, trim silence,
  normalise to 95 %). It enforces a per-clip length limit: 4 s, the boot-scene spec's
  "clips are ≤ 4 s" (the builder can revisit this).
- It writes the three `.snd` files and prints each clip's length.
- A committed `data/rosters.example.txt` documents the format without personal paths.
- Python unit test (`tools/test_gen_rosters.py`, as the other `tools/test_*.py`): the format,
  the length limit, a missing WAV failing clearly.
- The owner's own `data/rosters.txt` is written once from his picks above.

### Picking: `util/black/soundpick` (pure, host-tested)

- `int pick(uint32_t rnd, int last, int count)`: a random index in `[0, count)` that is never
  `last` when count > 1. An out-of-range `last` counts as none. count 0 → -1.
- **Fitting the montage start before the logo:** the logo clip is picked first. The
  montage-start clip is then picked only among the clips that `bootanim::openerFits` accepts
  for that finale. If none fits, there is no montage-start sound, as today. The pure function
  takes a predicate or a list of lengths. The builder chooses which.
- `main.cpp` reads and writes the last picks in NVS namespace `boot`, keys `drive`, `opener` and
  `finale`, next to `last` (the opening). For the drive, the pick and the NVS write only happen
  when the opening is the Honda.

### Playing: `audio/greeting`

- `Clips` gains the three rosters. `greetingClip()` returns the picked drive clip for
  `intropick::MAIN`. The finale and opener are the picked clips.
- **Fallback:** if a `.snd` is missing or invalid, that slot uses today's file (`greeting.pcm`,
  `opener.pcm` or `finale.pcm`). With neither, the built-in chime (drive) or no sound (opener,
  finale), as today.
- `durationMs()`, `level()`, `finaleMs()` and `openerMs()` report the picked clip, so the boot
  scene's timing (exit gating, the finale ending with the fade, the opener-fits check) needs no
  change.
- The serial log names the picks, e.g. `[boot] sounds: drive=ami_okkaeri_01 opener=... finale=...`.

### Test hook

- `-D SOUND_FORCE=\"drive:3,opener:0,finale:11\"` (or any subset) forces roster indices for a
  desk check. It goes in ARCHITECTURE.md's Test hooks table. `INTRO_FORCE=\"honda\"` still
  forces the opening.

## The on-screen greeting from branch `claude/intro-greetings-8b8b67`

That branch drew a random Japanese greeting in the sky during the Honda drive. The owner chose
to drop it: the voice is what he wanted varied, and text that disagrees with the clip would look
wrong. So this work starts from `main`, and that branch stays unmerged. No clip carries display
text, and no clock rule applies to the audio: the owner picked every clip by ear, so an "ohayo"
can play at night.

## Credits and docs

- **CREDITS.md:** "声: あみたろの声素材工房" (amitaro.net, credit required). Pocket Sound
  (pocket-se.info) and 効果音ラボ need no credit for personal use, but the builder lists them
  too. The myinstants clips are personal use, not redistributed.
- **ARCHITECTURE.md:** the Greeting bullet and the media list. **CHANGELOG.md:** [Unreleased].
  Update the `media_sizes.txt` record (`tools/media_sizes.py`).

## Done when

- `tools/run_tests.ps1` passes, including the new `test_soundpick` and `test_sndroster`, and
  `python -m unittest` passes for the tools tests.
- `pio run -e s3` builds with -Werror.
- The pack (`pio run -e s3 -t uploadmedia`) fits the partition.
- On the desk, after the owner OKs a flash on COM12: several boots log different picks per
  slot, with no slot repeating its previous boot's pick, and the opener is never cut off by the
  finale.

## Left to the builder

- The exact `.snd` header layout and the predicate-or-lengths API.
- Whether the 4 s limit stays.
- How `SOUND_FORCE` is parsed.
- The order of NVS writes.

## Calls made in writing this (check these)

- One `.snd` file per slot because of the 32-file pack limit (an implementation call, not the
  owner's).
- `data/rosters.txt` is git-ignored and lists the owner's picks. The `now_*` WAVs live in
  `greeting-sounds/listen/`, so their paths there are `listen/now_*.wav`.
- Both Bem-vindo entries stay in the drive roster (today's cut and the clean cut), as ticked.
- Opener vs finale: the montage is 71 frames at 10 fps (7.1 s). Even with the longest logo
  clip (3.78 s) the opener has 3.0 s, and the longest montage-start clip is 2.55 s. So every pair
  fits today. The fit filter guards against future clips.
