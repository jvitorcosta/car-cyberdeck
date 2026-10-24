# Music mode for the car lights (design)

Date: 2026-10-24. Builds on the yellow deck's LED panel (`src/ledpanel.cpp`, env `ledpanel`, merged in #13/#18) and the black deck
(S3, `src/main.cpp`, env `s3`). Related: the paused music-mode brainstorm, the voice-agent mic findings (branch `feat/voice-agent`).

## Intent

The owner wants the car's LED lights to follow the music playing in the car: **colour, intensity and pulse**. Spotify no longer gives
tempo/beat data to new apps, and the owner rejected a BPM-only version (Deezer BPM gives tempo but not beat positions). So the lights
react to the **sound in the cabin**, heard by the black deck's microphone, and take their **colours from the song** (album cover, genre)
adjusted by the **time of day**.

## Goals

- A **MUSIC mode** for the lights, switched on from the yellow deck (Lights page), by voice ("Honda, music") and from the watch, and
  off by choosing any colour, preset or Off (any manual change wins).
- **Inside light:** pulses on detected beats, intensity follows loudness, colour moves through the song's palette with the music's energy.
- **Outside light:** steady in the song's main colour, changing only gently when the song changes (Brazilian traffic law forbids
  flashing coloured lights outside a moving car; owner's decision).
- Works with the **onboard microphone** as it is today (weak, inside the Game Boy shell) at the owner's normal listening volume
  (the car stereo at about 18 of ~40, mid volume), and gets better, with no redesign, with a better mic later.

## Non-goals

- No BPM lookup (Deezer etc.). No Spotify audio-analysis (closed to new apps).
- No new microphone hardware, no shell modification, no new mic test in this version (owner: design from the data we have).
- No music mode for the outside light's pulse. No strobe effects. No ELK built-in effect modes.
- The voice agent (same mic, other branch) is not part of this; the two must not fight over the mic (see Constraints).

## Anti-goals (a failure even if it works)

- **Flicker or seizure-like flashing**: brightness steps faster than ~4 Hz at full depth, or random jumps when there's no music.
- **A dead-looking mode**: lights frozen or going dark when the music is quiet; quiet must look calm, not broken.
- **The lights get worse**: music mode must not slow taps, voice or the watch, or drop the BLE links.
- **The black deck gets worse**: Spotify polling, lyrics and its screen keep working (its TLS heap is tight; an SD card already starved
  it once).
- **Outside pulsing** while driving.

## What we know about the microphone (no new test; facts from 2026-10-22/23)

- ES8311 onboard mic on the black deck: I2S 16 kHz, gain +42 dB (reg 0x16 = 7) and ADC volume +24 dB (0x17 = 0xEF) were needed;
  at 0 dB ADC volume everything sat at the noise floor.
- At those settings the **hum floor is RMS ~400-750** (of 32k), mostly low-frequency; **speech at arm's length ~1.5-3x** the hum;
  **close/loud sounds (claps) ~22x** (peaks ~21k). Left and right channels are identical.
- Phone music at arm's length didn't rise above the hum. In-car music at mid volume through the car speakers is untested; expectation:
  bass/kicks several times the floor, treble weak (the shell blocks high frequencies most).
- The mic shares the S3's I2S with the speaker path (ES8311 full duplex: the greeting, the music box).

## Design

### The two decks' jobs
- **Black deck (listener + colour source):** captures the mic, extracts features, computes the song's palette, and **sends** both to the
  yellow deck. It does this **only while the yellow deck has music mode on** (mic and analysis off otherwise).
- **Yellow deck (light driver):** turns the features into light commands at a safe rate, owns the mode, the UI and the voice/watch commands.

### Features (black deck, ~30 per second)
- Capture 16 kHz mono, frames of 512 samples (~32 ms), Hann window, FFT (or a few IIR band filters if cheaper).
- **Bass band ~70-180 Hz** (kick drum; above the engine/electrical hum), **mid band**, **treble band**.
- **Adaptive noise floor** per band: a slow follower of the band energy's lower percentile (minutes when quiet, held while music plays).
- **Beat**: spectral flux in the bass band above `k × median(flux over the last ~1 s)` and above the noise floor, with a refractory
  period of ~250 ms (≤4 beats/s).
- **Energy**: overall band energy above the floor, smoothed (fast attack ~50 ms, slow release ~400 ms), normalised against a rolling
  peak over ~10 s (so it uses the full 0..1 range at any volume).
- **Brightness**: treble / (bass+mid) ratio, smoothed (only a weak hint with this mic).
- **Signal present**: energy above the floor for > ~1 s in the last 3 s.
- Sent as a small UDP datagram to the yellow deck: `{seq, beat (0/1 + strength), energy 0..255, brightness 0..255, present}`.

### Palette (black deck, once per song)
- **Album cover**: from the art the black deck already decodes (images/art, tjpgd), a coarse colour histogram (e.g. 4096 buckets or a
  12×6 hue×lightness grid) skipping near-black/near-white/low-saturation pixels; pick the 3 most weighted distinct hues (≥ 30° apart),
  each at full saturation and value.
- **Genre**: the Apple genre the black deck already looks up (util/genre, the badge) maps to a colour family (rock/metal reds-oranges;
  electronic/dance blues-purples-cyan; pop pinks-magentas; hip-hop/R&B/funk purple-gold; chill/lo-fi/acoustic/jazz soft teal + warm
  white; sertanejo/MPB/samba/pagode/forró warm amber-green; unknown: neutral).
  - Colourful cover: nudge each palette hue ≤ 15° toward the genre family's nearest hue.
  - Grey/washed-out cover (fewer than 2 usable hues): the genre family's 3 colours replace it.
- **Time of day** (the black deck's clock, same 18:00-06:00 night as the decks): day as is; evening/night shift warmer (blues toward
  purple, whites toward amber) and cap the brightness; after 22:00 a lower cap and softer pulses.
- Sent to the yellow deck as `{song id, 3 × RGB, brightness cap, pulse depth}` at each new song (and resent every ~10 s).

### Light behaviour (yellow deck)
- **Inside light, at ≤ 15 writes/s** (the ELK controllers' comfortable rate; both header variants are already sent per colour change —
  in music mode send only the variant the controller answers to if known, else alternate, to halve the traffic; builder to decide):
  - base colour = the palette colour picked by energy over ~8 s (calm → the palette's most muted, loud → its brightest);
  - a **big change** (energy jump ×2 within 2 s after a calm stretch) moves to the next palette colour **on that beat**; otherwise colours
    crossfade over ~2 s;
  - **beat pulse**: brightness jumps up by `pulse depth` and decays over ~200 ms; no pulse faster than the 250 ms refractory;
  - intensity = cap × (0.35 + 0.65 × energy), so quiet = calm glow, never off;
  - no signal: settle in ~3 s on the palette's first colour at a soft steady level.
- **Outside light**: the palette's first colour at a steady level (cap × 0.7), crossfading over ~5 s on a new song. No beat, no energy.
- **Exit**: any colour/preset/Off/level change from any source (touch, Xbox, voice, watch) leaves music mode and applies that change.

### UI and commands
- **Lights page:** a `MUSIC` control, a fifth preset-style slot or a toggle on the zone row (builder's call, ≥ 70×36, fits the layout).
- **Dashboard:** the light rows show `MUSIC` and the live colour chip; the messages row shows `>MUSIC ON` / `>MUSIC OFF`; while on and the
  black deck isn't heard: `>NO SIGNAL` (normal, not ALERT).
- **Theme**: while music mode is on, the deck's theme follows the **song's main colour** (changes once per song), not the live inside
  colour (which changes many times a second).
- **Voice:** "Honda, music" (and "music mode", "música"? no: English only, owner rule) turns it on; any colour command turns it off.
- **Watch:** a MUSIC item on the PRESETS page (or BOTH page's list); `/say?text=music` works.
- **State**: `/status` gains `music=on|off` (append, keep the format readable; the watch ignores unknown keys).

### Transport between the decks
- Yellow → black: `GET http://<black deck>/music?k=…&on=1|0` (on toggling, and repeated every 10 s while on as a keep-alive; the black
  deck stops capturing after 30 s without one). Host: the existing lyrics-task host logic (hotspot fixed address / mDNS).
- Black → yellow: UDP to the yellow deck's address (it sends the request, so the black deck answers to the request's source IP) on a fixed
  port, features ~30/s, palette on change and every ~10 s. Lost packets are fine (stateless, latest wins).

## Constraints
- Black deck: Spotify TLS heap is tight — capture/FFT buffers static and small (no extra TLS, no large allocations while music mode runs);
  watch the `[mem]` line. I2S full duplex: the greeting / music box must not break capture (music mode pauses the music box, the greeting
  only plays at boot).
- The voice agent (`feat/voice-agent`, not merged) will want the same mic: design the capture module so both can use it later (one owner of
  I2S RX; features and voice as consumers). Out of scope to implement the sharing now.
- Yellow deck: BLE writes ≤ 15/s per controller; music mode must not starve the link task, the voice server or touch.
- Night mode/backlight unchanged; the time-of-day colour logic lives on the black deck.
- Public repo; English voice only; `-Werror`; pure logic host-tested (features, beat detector, palette extraction from a pixel buffer,
  genre map, time-of-day adjust, light-behaviour state machine).

## Decided in the conversation (the owner's)
- A mic-driven music mode; not Spotify BPM, not Deezer BPM.
- No new mic test; design from the data we have; onboard mic now, better mic later without redesign.
- Colours shift with the music; the palette comes from the album cover and genre, adjusted by time of day ("do some heuristics").
- Inside light dances; **outside stays steady** in the song's main colour.
- Normal listening volume: car stereo ~18 (mid).

## Left to the builder
The exact band edges, thresholds and time constants (start from the values above, make them constants, tune later); FFT vs filters;
the UDP format; the MUSIC control's place on the Lights page; how the header-variant traffic is halved; the genre→family table details.
