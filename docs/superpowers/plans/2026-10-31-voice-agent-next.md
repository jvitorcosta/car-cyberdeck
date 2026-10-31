# Voice agent: what comes next

**Date:** 2026-10-31
**Status:** backlog. Starts when the new deck arrives (the JC4827W543 handheld, ordered 2026-10-29).
**Builds on:** `feat/voice-agent` (local, not pushed): the spec
`docs/superpowers/specs/2026-10-23-voice-agent-design.md`, the mic test (part 1) and the cloud
agent in `agent/` (part 2).

This is a roadmap, not a task-by-task plan. Each phase gets its own detailed plan when it starts,
against the code of that day. Private details (which notes are shared, where each kind lands) stay
out of this public repo: they live in one allowlist file on the owner's PC.

## Why

v1 makes Zero Two a faster, Portuguese Siri for the lights and Spotify. The owner wants it to manage
more from the driver's seat and leave a trace in their personal notes, without putting anything
private at risk. The rules from v1 hold throughout: one short sentence, nothing to read on screen,
it speaks first only for real car alerts, nothing is sent without a spoken yes, and a wrong action
is the worst failure.

## Phase 0: make it speak (needs the new deck)

Nothing below reaches the car until this phase is done.

1. **Check the board:** confirm the JC4827W543's pinout and audio. Find out whether it has a mic; if
   not, wire the INMP441 on I2S.
2. **Decide where the agent lives:** the new deck, or the black deck with the mic beside it. Xbox
   hold-to-talk (RT to talk, LT to cancel and undo) has to reach whichever it is.
3. **Prove the mic:** the 10-clip recording and the speech-to-text bench, then pick the provider.
4. **Run part 2 live:** the steps listed in `docs/BACKLOG.md`.
5. **Write and build part 3:** audio to and from the agent, the dialog box, hosting the service.

## Phase 1: side brain, and the bridge to the notes

No more hardware needed. It depends on part 2 being live.

**The bridge.** The notes stay on the PC and the agent runs in the cloud, so it reaches them through
GitHub, with the PC as gatekeeper:

- **Notes backup repo:** private. Only the owner pushes to it and the agent has no access.
- **`car-agent` repo:** private, and the only thing the agent's token can reach.
  - `inbox/`: the agent drops new files here.
  - `context/`: the PC exports an allowlist here, and the agent reads only this.
- **Import and export on the PC:** part of the daily routine. It takes new `inbox/` files, validates
  them, writes them to their allowed places, commits, then exports `context/`.

**Features:**

| Feature | Example | Reads | Writes |
|---|---|---|---|
| Voice notes, tagged to a project | "anota: ideia pro deck, LEDs piscando no freio" | project names and status | a new inbox note |
| Reminders, confirmed aloud | "me lembra de trocar o óleo dia 20" | nothing | a calendar event and an inbox note |
| Your day | "qual o próximo compromisso?" | calendar, e-mail subjects, today's plan | nothing |
| Study on the road | "me faz 3 perguntas da aula" | the study plan | nothing |

## Phase 2: music, lights, scenes

Builds on the Spotify tools, `/say` and the #23 song colours.

| Feature | Example | Reads | Writes |
|---|---|---|---|
| Teach song colours | "essa música é roxa" | the picks list | a new pick |
| DJ by mood, learning from skips | "algo pra estrada" | Spotify | songs liked in the car |
| What's playing | "que música é essa?" | Spotify | nothing |
| Scenes: lights, playlist, screen, tone | "modo chuva" | scene definitions | a new scene |

## Phase 3: the car

Needs the OBD deck (`feat/car-health`, draft #30) and the Vgate iCar Pro BLE. The OBD deck reads the
car and owns health and the trip log; the agent asks the decks, because the cloud can't reach the car.

| Feature | Example | Reads | Writes |
|---|---|---|---|
| Health in Portuguese; warns on its own only for real problems | "como tá o carro?" | the car's notes | fault codes and warnings |
| Trip summary when parking | "hoje foi tranquilo, 23 km" | nothing | a trip line |
| Correct a fact | "troquei o óleo hoje" | nothing | a proposal the owner accepts |

## Phase 4: WhatsApp

Comes last. Reading messages needs Bluetooth on the deck (ANCS), which may not fit beside Spotify's
TLS heap. It reads new messages aloud and replies only after a spoken "confirma?". The only thing
that reaches the notes is a "responder a <name>" reminder, **never message text**: those are other
people's words, and the inbox passes through GitHub.

Heart-rate features and the Pokémon XP game were not picked; they can come back later.

## Guardrails

Free private GitHub repos have no per-path rules or branch protection, so code enforces these:

1. **Create and append only.** Each agent tool writes one new file under `inbox/`. No tool edits,
   renames or deletes anything, and paths outside `inbox/` are refused in code, with tests.
2. **A minimal token.** Fine-grained, `car-agent` only, contents read/write, 90-day expiry.
3. **The PC is the gatekeeper.** It imports only files it hasn't seen, validates each against a
   schema for its kind (allowed target, required fields, size cap, no links outside the allowed
   targets), and refuses and logs anything else. Exports come from one allowlist file, so giving a
   feature read access is one reviewed line.
4. **Reminders don't depend on the PC.** The agent creates the calendar event directly, so it fires
   even if the PC is off for days.
5. **Private areas stay out.** Anything not on the allowlist is never exported. A note about a
   private topic waits in the inbox for the owner to sort.

## To decide in each phase's plan

- The import/export script's language and trigger; the schemas; the `inbox/` file names; how the
  agent's tools batch commits.
- Calendar and e-mail access from the agent's server (API and OAuth).
- How a lesson becomes a 2-minute spoken summary or a 3-question quiz.
