# IR remote on the yellow deck

## Why

The car's outside LED kit came with a 24-button IR remote, but the kit's own IR receiver sits outside
the car, so the remote does nothing from the seats. A KY-022 receiver is now wired to the yellow deck
(CYD). The owner will mount the remote on its own phone holder on the dash, next to the decks. It is
a fixed control panel, not handed around: you reach over and press, like the deck's touch screen, but
with physical buttons ("physical buttons are way cooler"). What he reaches for first is dimming the
lights and turning them off; the music buttons start the playlists he plays most.

Hardware, wiring, the remote's layout and the test sketch are in the earlier handoff:
`docs/superpowers/plans/2026-11-12-ir-remote.md` (KY-022 on CN1, signal on GPIO 27, 3.3 V;
`src/diag/irtest.cpp` and `[env:irtest]`, kept in `git stash` from `feat/irtest`). That file's
"Proposed mapping" is replaced by the one below.

## Goals

1. Every button acts on both lights together, as the BOTH page does, from any screen.
2. Brightness and on/off act on the first press, every time, with the remote aimed from the driver's
   seat at the deck a hand's width away.
3. The four music buttons each start one of the owner's playlists on Spotify, shuffled.
4. The JUMP / FADE buttons run the lights' own effects, like the kit's remote would.

## Non-goals

- Music mode and karaoke on the remote (they stay on the deck, voice, the watch and the PS3 pad).
- Learning other remotes on the deck; the codes are fixed in firmware.
- Sending IR; the black deck; a second remote.

## Anti-goals (a failure even if it works)

- A press that does nothing, or acts twice. Brightness and OFF must never feel unreliable.
- Another device's IR remote (a TV, the car's own kit remote if it ever reaches the deck) changing
  the lights: only this remote's address counts.
- The yellow deck less stable than today (heap ~30 KB low water, Bluedroid dual mode).

## The mapping (agreed)

| Remote | Does |
|---|---|
| ☀▲ / ☀▼ | both lights one 10 % step up / down; held, it keeps stepping (about 4 steps a second) |
| ON / OFF | both lights on / off |
| RED, GREEN, BLUE, WHITE, ORANGE, YELLOW, CYAN, PURPLE | that colour on both lights; each keeps its own level, and lights that are off come on |
| JUMP3, JUMP7, FADE3, FADE7 | the controllers' built-in effects (jump through 3 / 7 colours, fade through 3 / 7) on both lights |
| ♪1 | the playlist "Blinded by the Lights", shuffled |
| ♪2 | "Divas Pop", shuffled |
| ♪3 | the driving playlist the owner calls "Through Goes Hamilton" (exact name to match in his library) |
| ♪4 | "Cuddles and Moans" (likely "Amassos e Gemidos" in his library) |

- **Colour, level and on/off end music mode,** like any manual change (`musicYield`), and an effect
  is a manual change too. A colour, level step or on/off press ends a running effect (a colour frame
  ends it on the controller, as `elkcmd::effect` notes).
- **Feedback** is the deck's usual terminal message (`>BLUE`, `>LIGHTS OFF`, `>FADE7`,
  `>PLAYING DIVAS POP`, `>BLACK DECK NOT ANSWERING`), plus the lights themselves.
- **Shuffled** means: Spotify's shuffle turned on and playback starting at a random track of the
  playlist, so each press starts somewhere different.

## How the pieces fit (the owner's calls and the existing paths)

- **Receiving:** IRremote 4.4.1 on GPIO 27, wrapped in a private library under `lib/` so `src/` keeps
  `-Werror` (the handoff's Task 3 explains why). The pure mapping goes in `src/util/yellow/` with a
  host test, like `padguard` and `padfeel`.
- **Codes:** captured first, with the owner, by flashing `irtest` to the yellow deck over USB and
  pressing each button once (the handoff's Task 1). Expected NEC, one address; that table lives in
  the mapping header.
- **Lights:** the same calls the PS3 pad uses (`change(BOTH, ...)`, `bothlook::recolour`, `push`,
  `save`, `drawLooks`). A colour press is the pad's flick with a fixed colour, plus "on".
- **Effects:** `elkcmd::effect(mode)` and `elkcmd::speed(pct)` to each controller, as the welcome
  curtain does. Which `mode` numbers are jump 3 / jump 7 / fade 3 / fade 7 is not known; find them
  in the car by trying modes, starting from the MELK lists (lotus-lamp-python). The outside
  controller takes ~1 write a second, so the two frames to it are spaced like the preset fade.
- **Playlists:** the yellow deck asks the black deck, signed like its other asks (`deckGet`), at
  `/playlists/play?id=<id>`, which the watch already uses. The black deck gains a shuffle option on
  that endpoint (shuffle on, then play from a random position). That means a black-deck change and
  a black-deck flash, which the owner approves each time.
- **Playlist IDs** stay out of the public repo: four IDs in the git-ignored `src/config.h`
  (with empty placeholders in the example config); a missing ID makes its button show
  `>NO PLAYLIST SET`. The builder finds the IDs in the owner's library (the black deck's
  `/playlists` page, or the Spotify web player) and confirms the four names with the owner.

## Constraints

- Heap: no new task, no buffers over a few hundred bytes; the IR receive runs off a timer interrupt
  (IRremote's 50 µs sampling) and is polled from `loop()`. Check `[mem]` before and after.
- The black deck ask runs on the existing `deck` task, never in `loop()` (an HTTP call blocks up to
  1.5 s).
- Public repo: no playlist IDs, MACs or personal notes in committed files.
- Flash the yellow deck over USB only; the black deck only with the owner's OK each time.
- Conventional commits; docs in `docs/yellow-deck.md` (a "Remote" section) and `CHANGELOG.md`.

## Calls made after the builder check (the designer's, for the owner to check)

- **Address filter:** any remote with this remote's NEC address counts. Generic 24-key light
  remotes share one address, so a second identical remote (another kit's) would work too; that is
  fine for a fixed panel in one car.
- **Effects on both lights:** the inside controller already runs built-in effects (the welcome
  curtain). The outside one is expected to (MELK); if it ignores effect frames in the car test, it
  keeps its colour and only the inside light runs the effect. No effects driven from the deck.
- **Shuffle stays on** after a ♪ press, as it would if he'd tapped shuffle in Spotify; later
  playlist picks (the watch) then play shuffled too.
- **The ♪ ask waits for the black deck:** one ask on the `deck` task, with its own longer timeout
  (16 s: the black deck itself waits up to 15 s on Spotify for the track count, play and shuffle), then `>PLAYING ...` or the honest failure. The
  keep-alives wait behind it; a missed one is retried on the next round as today.
- **Heap gate:** the low water since #57 is ~30 KB at its worst (music mode streaming). Record
  `[mem]` with and without IRremote; if the low water drops more than ~1 KB, stop and look before
  going on.

## Left to the builder

- The repeat rate and the threshold for "held" (NEC repeat frames come every ~110 ms).
- The effect speed, and how an effect shows on the Lights page.
- How a press is logged (`[ir] ...`), and ignoring foreign codes quietly after logging each once.
- How the remote's mount is aimed; whether the receiver needs a hole in the deck's case (with the
  owner, on the car).
