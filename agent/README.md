# Voice agent

A small TypeScript service that turns a text command ("toca Djavan", "põe uma do Djavan e depois
aumenta o volume") into Spotify actions, car lights and a short spoken-style reply in Zero Two's voice.

- **Fast path:** one Claude Haiku call with tools. It handles most commands.
- **Slow path:** the Claude Agent SDK (Sonnet) for multi-step commands the fast path hands off.
- **Traces:** one JSON line per command in `agent/traces/YYYY-MM-DD.jsonl` (git-ignored).

This is part 2 of the voice agent: text in, reply out. Part 3 adds audio (speech-to-text and her
voice), the deck's `/talk` endpoint and WebSocket, and deployment.

## Set up the API credits

The Claude Max 5x plan includes monthly API credits (US$100). They are linked to a Claude Console
organization, so the agent does not use your plan's chat limits.

1. At claude.ai go to Settings, Billing, API credits and link the credits to a Console organization.
2. In the Console, create a dedicated workspace for the agent and set a spend limit on it.
3. Create an API key in that workspace. It goes in `agent/.env` as `ANTHROPIC_API_KEY`.

When the credits run out, requests stop. Nothing is charged to the plan.

## Run it

From the repo root, get a Spotify token with the playlist scopes. This opens the consent page and
writes the Spotify values into `agent/.env` (created from `.env.example` if missing):

```
python tools/spotify_auth.py --agent
```

Then fill the rest of `agent/.env`: `ANTHROPIC_API_KEY` and `DECK_SECRET` are required. `FAST_MODEL`,
`SLOW_MODEL` and `PORT` have defaults (see `.env.example`). Then, from `agent/`:

```
npm install
npm test
npm start
```

Try it from another terminal (use your own `DECK_SECRET`):

```
curl -X POST http://localhost:8787/command \
  -H "Authorization: Bearer $DECK_SECRET" \
  -H "Content-Type: application/json" \
  -d '{"deck":"yellow","text":"toca Djavan"}'
```

The reply has `heard`, `lights`, `stock`, `say`, `mood`, `path` and a `traceId`. `deck` is 1 to 32
characters of `a-z`, `0-9` and `-`; `text` is at most 500 characters.

## Endpoints

| Method and path | Auth | What it does |
|---|---|---|
| `GET /health` | none | `{ "ok": true }` |
| `POST /command` | Bearer `DECK_SECRET` | Runs one command |
| `GET /stats?days=N` | Bearer `DECK_SECRET` | Counts, latency and cost over the last N days (1 to 90) |

## Stats

```
curl -H "Authorization: Bearer $DECK_SECRET" "http://localhost:8787/stats?days=7"
```

It returns `n`, `p50Ms`, `p95Ms`, `costUsd` and `byPath` (commands per path). Days are UTC days, so
"today" rolls over at 21:00 in Brasília. Prices live in `src/trace.ts`; update them there when they change.

## Evals

The evals run the real model over `evals/cases.jsonl` and score what it does. They cost real
credits, a fraction of a cent on Haiku. Each run appends a row to `docs/voice-agent-evals.md`
(read it for how verdicts are scored).

```
npm run eval -- --model claude-haiku-5-5
npm run eval -- --model claude-sonnet-5-5
```

Guard-only cases (empty or too-short text) never reach the model, so they cost $0 and take 0 ms.

## First live steps (not done yet)

- [ ] Get the Spotify token: `python tools/spotify_auth.py --agent`
- [ ] Fill `agent/.env`
- [ ] `npm start`, then the `curl` "toca Djavan" check. Also confirm adding to a playlist works:
      Spotify may have renamed the playlist tracks endpoint to `/items`.
- [ ] Smoke-test the Agent SDK slow path: "põe uma do Djavan e depois aumenta o volume"
- [ ] Run the first baselines: `npm run eval -- --model claude-haiku-5-5`, then `claude-sonnet-5-5`
- [ ] Look at `cache_read_input_tokens` in the traces (`cacheReadTokens`). If it stays 0 the
      prompt may be under Haiku's minimum cacheable length.
