# CarDeck: notes for coding agents

Firmware for the car decks (black ESP32-S3, yellow ESP32 CYD, OBD ESP32) that read OBD-II from a
Honda City over a Vgate BLE adapter. See README.md and CONTRIBUTING.md for the project itself.

## Project skills (`.claude/skills/`)

- `obd2`: OBD-II / ELM327 / PIDs / the safety gate. Load for any OBD work.
- `esp32-dev`: building, flashing, heap, tasks, watchdog, partitions. Load for any firmware work.

## Reviewer subagents (`.claude/agents/`)

Both are read-only (Read, Grep, Glob, Bash for builds and tests). Give them a PR number, a branch
or a commit range. They return findings ranked by severity with `file:line` and a one-line fix,
plus a "verified by" list.

| When | Call |
|---|---|
| A diff touches OBD, ELM327, BLE-to-adapter, PID, polling-plan or gauge code (`src/obd/`, `src/util/car/obd*`, `tools/obd*.py`, `tools/obd2/`) | `obd2-reviewer` |
| Any firmware diff before merging or flashing (`src/`, `lib/`, `platformio.ini`, partitions, build or flash tooling) | `esp32-reviewer` |

A change that touches both (a new gauge, say) gets both. The reviewers never flash, upload or open
a serial port; flashing a deck stays a manual step the owner asks for, with the right env and port.
