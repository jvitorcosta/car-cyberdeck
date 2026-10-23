# Voice agent on the black deck (Zero Two)

**Date:** 2026-10-23
**Status:** draft for review
**Builds:** `s3` (black deck), `ledpanel` (yellow deck, small change), plus a new cloud service

## Why

The owner controls the car lights by voice through a Siri Shortcut ("Honda" → `GET /say` on the
yellow deck, `docs/leds-voice.md`). Siri is **too slow** and **sometimes doesn't understand** him,
and with Siri in English he can't speak Portuguese (the 2026-10-22 "voice stays English" decision
was only a Siri limitation). He wants a voice assistant built on the Claude Agent SDK that replaces
the Shortcut, speaks Portuguese and English, and can grow into managing more things while he drives.

## Goals (v1)

- Replace the Siri Shortcut for the car lights, in **Portuguese** (v1 is pt-BR only, see below)
  ("deixa azul fraquinho dentro, fora desliga").
- Control Spotify by voice: play/pause, next/previous, volume, search and play ("toca Djavan",
  "põe o álbum novo do Tyler"), add to queue, save the current song to a playlist, "what's playing".
- Talk with the push of a button on the Xbox controller.
- Answer as **Zero Two**: an anime-style voice and her portrait and dialogue on the black deck.

## Anti-goals (what would make him go back to Siri)

The owner named two. These come before everything else:

1. **Slow.** Target: the lights or Spotify change **≤ 1.5 s** after RT is released; **2.5 s** is
   the hard ceiling for simple commands. Every stage is timed and logged on the server (see Speed).
2. **Wrong action.** Plays the wrong artist, turns the lights off when he asked for blue, and fixing
   it takes longer than doing it by hand. Rules:
   - What she heard always shows in the dialogue box, so a mishearing is visible at once.
   - When she isn't sure (two artists that sound alike, a transcript that doesn't make sense, an
     unknown playlist), she **asks one short question** instead of guessing ("Djavan ou Djonga?").
     He holds RT again to answer.
   - **Undo:** LT within 10 s of an action, or saying "volta" / "desfaz" / "undo", puts back what
     she changed (the light colours and levels from before, or the Spotify context/track and
     position from before).

Also avoided (my calls): chatter (replies are one short sentence, usually a stock phrase), and
anything that pulls his eyes off the road (no long animations, no text he has to read to know it
worked).

## Non-goals (v1)

- English commands. **v1 is Portuguese only** (owner, 2026-10-23): one language makes speech-to-text,
  the prompt, stock phrases and the evals simpler and more accurate. English and mixed sentences
  come later; only English names inside a Portuguese command are handled now.
- WhatsApp (the later route is Apple's ANCS notifications over BLE on the S3, read-only, after a
  heap test; the unofficial WhatsApp libraries risk a ban and are rejected).
- Distance/ETA (Google Routes API; needs a location source: a GPS module, an iPhone Shortcut, or
  asking). Waze has no public API.
- Sending anything on his behalf. When it comes later, it always gets a spoken "confirm?" first.
- Wake word, tap-to-talk with silence detection, a steering-wheel button, a physical button on the
  deck (he'll add a keyboard or button later; the design takes a generic "talk" signal for it).
- Long-term memory across drives.

## How it fits together

```
Xbox (RT/LT) ──BLE──▶ yellow deck ──HTTP over hotspot──▶ black deck ──TLS──▶ cloud agent
                       (ledpanel)    /talk?state=...        (S3)       audio up,   │
                           ▲                                  │        reply down  ├─▶ Spotify Web API
                           └──────── GET /say?zone=... ◀──────┘                    ├─▶ speech-to-text
                                     (lights, unchanged)                           ├─▶ Claude
                                                                                   └─▶ text-to-speech
```

### Yellow deck (`ledpanel`) — the controller stays here

- The Xbox controller stays paired with the yellow deck. The S3 gets **no Bluetooth**, so its heap
  stays free for TLS (the Spotify -76 errors show it's already tight).
- RT pressed → `GET http://<black deck>/talk?state=down&k=<REMOTE_TOKEN>`; RT released →
  `state=up`; LT → `state=cancel` (also undo, see above). Sent without blocking the panel's loop.
- RT and LT are free today (`src/ledpanel.cpp:551`: D-pad, A, B, X, Y, LB, RB are taken; on
  `feat/yellow-menu` B is Back). Menu/View stay free for direct Spotify keys later.
- The black deck's address on the hotspot: the yellow deck is always `172.20.10.11`; the builder
  finds a stable way to reach the black deck (it may also get a fixed address, or mDNS).
- `/say` and `/state` are **not changed**. Siri, the watch and touch keep working.

### Black deck (`s3`) — ears, mouth and face

- **`/talk` route** on the existing remote web server (`src/net/remote.cpp`, same `REMOTE_TOKEN`).
  A later physical button calls the same code path.
- **Listening:** on `down`, open one connection to the agent and stream the ES8311 mic at 16 kHz,
  16-bit mono while RT is held; on `up`, end the stream. Max 15 s.
- **One TLS connection at a time:** Spotify polling pauses while a talk session is open and resumes
  after. The S3 never holds two TLS sessions.
- **Lights:** the reply carries light actions (zone + the words `/say` already understands, e.g.
  `zone=inside&text=blue 30`); the black deck calls the yellow deck's `/say` locally, because the
  cloud can't reach the hotspot. It reads `/state` first, so undo knows the previous colours.
- **Speaking:** the reply's audio is 16 kHz PCM, played through `audiosink` (the same rate as the
  greetings and the music box). Stock phrases play from the media partition, with no network.
- **Zero Two on screen** (the 2026-10-14 avatar, `tools/gen_avatar.py`, 68 KB `avatar.bin`,
  moods `smug`, `happy`, `talk`): this reverses the 2026-10-14 "new deck stays as it was" call, on
  purpose. `avatar.bin` goes into the S3 media partition.
  - RT held: she slides into the album-art box (`smug`, listening).
  - Reply: her line types out in the battle dialogue box, first what she heard (small, dim), then
    her answer; `talk` frames flap while her audio plays (no real lip-sync).
  - Done: she holds ~3 s with `happy` (or `smug` on a question), then slides out and the battle
    screen returns. Her look can be refined later.
- **No server / no signal:** a stock "sem sinal~" line within 2 s; lights by touch and Xbox keep
  working.

### Cloud agent — the brain

- **TypeScript service built on the Claude Agent SDK**, with the tools below. Always on (no
  free tier that sleeps: a cold start breaks the speed goal), in the region closest to where he
  drives (Brazil). The host is the builder's choice.
- **Pipeline per command:** streaming speech-to-text while RT is held → model with tools → actions
  → reply text → stock phrase id or live TTS audio, all streamed back on the same connection.
- **Fast path = the router (owner, 2026-10-23).** No separate classify step. One model call gets
  the transcript, the short context and the tools, and either acts (`lights`, `spotify_*`, `ask`,
  `undo`) or calls `hand_off_to_agent(task)`. One call per simple command (~1 s total instead of
  ~1.5 s with a router in front). The fast path is our own plain tool loop on the model's API, not
  the Agent SDK. Risk: it tries a job it should hand off (e.g. "a que tocou antes do Djavan"); the
  evals carry such cases, and the prompt says that anything needing history or several steps is
  handed off. A separate router only comes back if the evals show the need.
- **Slow path = the Claude Agent SDK** for real jobs (several steps, later calendar, WhatsApp,
  reminders): the same tools as an in-process MCP server, hooks for "confirm before sending",
  Sonnet (`claude-sonnet-5-5`) by default. Zero Two says "deixa comigo~" while it works; its
  target is ~5 s, and after ~8 s she says it'll take a while and finishes in the background.
- **Model-agnostic fast path (owner, 2026-10-23):** the fast path calls its model through a small
  interface, so development can start on a free model (e.g. a Groq-hosted open model) and switch by
  config. The production model is picked by the evals (wrong-action rate first, then p95 latency,
  then cost), with Haiku, Sonnet and the free model as contenders; prompts and tool descriptions are
  re-checked on the chosen model. The Agent SDK slow path only runs Claude, so it is developed on
  Claude from the start (a small prepaid API credit).
- **Speech-to-text:** Brazilian Portuguese (v1), including English artist and song names inside a
  Portuguese sentence ("toca Tyler, the Creator"), biased with a word list; streaming.
  The builder picks the provider by measuring accuracy and latency on **recordings made in the car** (see the first step).
- **Voice (TTS):** ElevenLabs, a voice made with **Voice Design**: young, bright, expressive,
  anime-style girl, speaks pt-BR. Not cloned from any real voice actress. Its fast
  model, PCM output at 16 kHz.
- **Stock phrases:** the common replies ("hai~", "tocando", "pulando", "não entendi", "sem
  sinal", "desfeito", the colours) are generated once in her voice by a tool and packed into the
  media partition. Live TTS only for answers that change (a song or artist name, a question).
- **Personality (system prompt):** Zero Two style, cheerful and a little smug, replies in
  Portuguese, **one short sentence at most** and usually just a stock phrase; a sprinkle of
  "hai~", never a joke when he asked for an action.
- **Short context:** the last few exchanges are kept for 2 minutes so follow-ups work
  ("mais forte", "não, a outra", the answer to her question). Nothing kept between drives.
- **Tools (v1):**
  - `lights(zone: inside|outside|both, colour?, level?, power?)` → a light action for the deck.
  - `spotify_*`: play/pause, next/previous, volume, search + play (track/artist/album/playlist),
    queue, save current to a playlist, now playing.
  - `ask(question)` → she asks instead of acting.
  - `undo()`.
- **Spotify from the server:** the server has its own Spotify refresh token (same app, scopes for
  playback and playlist changes). The deck's firmware secrets stay on the deck.
- **Ducking:** on `down`, the server lowers Spotify's volume and restores it after the reply, so
  the mic hears him over the car speakers. Spotify plays on the iPhone (CarPlay), and the Web API
  often can't set volume on iOS devices (`supports_volume` false): then it **pauses** while he
  talks and resumes after.

## Security and cost

- `/talk` needs `REMOTE_TOKEN`. The deck authenticates to the server with its own shared secret over
  TLS.
- The Anthropic API key, the STT/TTS keys and the Spotify refresh token live only in the server's
  environment secrets. The repo is public: no secrets, no generated voice files in git (they are
  git-ignored like the other media).
- Billing: the Claude **subscription does not cover this**; it uses an API key from the Anthropic
  Console, billed per use, with a **hard monthly spending cap** set in the Console. STT and TTS are
  separate small bills.
- **Budget: US$10 per month, everything included** (host, Claude, speech-to-text, voice). The
  owner's number. It shapes the choices:
  - Haiku for nearly everything; Sonnet only when Haiku can't do the job, and logged.
  - Stock phrases cover most replies, so live TTS stays small enough for ElevenLabs' free tier or
    its cheapest plan (the builder confirms the tier allows Voice Design voices over the API, with
    PCM output).
  - The cheapest always-on host that meets the speed goal; the STT provider is chosen on
    accuracy first, then price at car volume.
  - The builder estimates the monthly cost at ~30 commands a day before building. If it won't fit
    in US$10, the owner decides what gives. Each provider gets its own hard cap or alert where it
    offers one.

## Speed

The server logs per command: audio end → transcript final, → model decision, → action sent,
→ first reply audio byte. The deck logs RT released → action applied. The 1.5 s target is
checked on real drives, not at home.

What 1.5 s measures (my call): for lights, the deck's `/say` call returning; for Spotify, the Web
API accepting the command, plus her stock phrase starting to play. Spotify Connect's own delay to
the iPhone (often 0.5–1 s) is logged but isn't counted against the target, because it can't be
controlled.

## Where it starts (my call)

- The `ledpanel` change (RT/LT → `/talk`) builds on `feat/yellow-menu`, which already polls the
  black deck and finds its address; it lands after that branch is tested and merged.
- The black deck needs `feat/hotspot-boot` (PR #12) to stay online in the car; the agent waits for
  it to be tested and merged too.
- The car mic test (first step) needs neither and can happen now.

## First step: prove the mic in the car

The 2026-10-22 mic spike (`spike/mictest-s3`, ES8311 settings in its notes) found a hum floor
(silence rms ~750 at max gain) and phone music close by didn't rise above it. Voice at arm's length
in a moving car with music is **unproven**. Before anything else: record a set of real commands in
the car (pt-BR, with English artist names; music on and ducked; windows up and down) from the deck's mic, and
run them through the candidate STT services. If accuracy isn't good enough, the plan stops and we
decide on a better mic before building the rest.

- **Pass bar (my call):** at least 9 of 10 commands transcribed well enough to act on, in each
  condition (music ducked/paused × windows up/down), with the best STT
  provider. The owner sees the results and makes the call.
- **The speaker too (my call):** the same drive checks that the S3's speaker can be heard over the
  road while Spotify is ducked or paused. If it can't, voice replies wait (screen and a beep only)
  and the TTS work moves later.

## Left to the builder

- Cloud host, STT provider (by the car recordings), the wire format between deck and server
  (WebSocket or chunked HTTP), how the black deck's hotspot address is found.
- The exact stock phrase list and the system prompt wording, within the personality above.
- Timeouts, retries and how the S3 schedules mic streaming next to the UI.
