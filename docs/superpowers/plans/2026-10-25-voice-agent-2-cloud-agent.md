# Voice agent, part 2: the cloud agent — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A TypeScript service that takes a Portuguese command as text and returns what Zero Two
does — light actions for the deck, Spotify changes it makes itself, and her line — through a
one-call fast path on Claude Haiku 5.5, a Claude Agent SDK slow path for multi-step jobs, a
2-minute session with undo, per-command traces, and a text eval set.

**Architecture:** `agent/` is a small Node service. `POST /command` takes `{deck, text}` and runs
the **fast path**: one Messages API call (Haiku 5.5, thinking off, effort low, strict tools) that
either picks a tool or calls `hand_off_to_agent`. Tools are plain functions shared with the
**slow path** (Agent SDK `query()` with the same tools as an in-process MCP server, Sonnet 5.5).
Her reply is built by code from the tool result (stock phrase id + short text), so a simple command
costs exactly one model call. Light actions go back to the deck, which calls the yellow deck's
`/say` itself (the cloud can't reach the hotspot). Every command writes one trace line.

**Tech Stack:** Node 18+ (the PC has 18.15), TypeScript, `@anthropic-ai/sdk`,
`@anthropic-ai/claude-agent-sdk`, `zod`, `vitest`, Node's built-in `http` and `fetch` (no web
framework).

**Spec:** `docs/superpowers/specs/2026-10-23-voice-agent-design.md`

**Out of scope here (part 3):** audio (speech-to-text, her voice, stock phrase audio), the deck's
`/talk` and WebSocket, ducking on RT press, Zero Two on the S3, RT/LT on the yellow deck, and the
cloud deployment. Part 2 runs on the PC and is reached over HTTP with text; part 3 puts audio in
front of `POST /command`'s logic and deploys it.

## Global Constraints

- v1 is **Portuguese only**; English artist and song names inside a Portuguese sentence work.
- **Speed:** a simple command must be decided in **one** model call; the fast path never does a
  second call for wording. Target for the whole pipeline is ≤ 1.5 s (spec), so the fast path's own
  budget is ~500 ms; every stage is timed in the trace.
- **Wrong action is the worst failure:** when unsure, call `ask(question)`; no tool call at all →
  stock `nao_entendi`, nothing changes.
- **Models (owner's choices):** fast path `claude-haiku-5-5`; slow path `claude-sonnet-5-5`.
  Model ids exactly as written, no date suffix. Both read from env (`FAST_MODEL`, `SLOW_MODEL`).
- **Fast-path request:** `thinking: {type: "disabled"}`, `output_config: {effort: "low"}`,
  `tool_choice: {type: "auto"}`, every tool `strict: true` with `additionalProperties: false`,
  `max_tokens: 1024`, system prompt + tools marked for prompt caching (`cache_control`).
- **Billing:** `ANTHROPIC_API_KEY` from the agent's own Claude Console workspace (Max 5x credits,
  with a spend limit). Never committed.
- **Secrets** (`ANTHROPIC_API_KEY`, `SPOTIFY_*`, `DECK_SECRET`) live in `agent/.env`,
  git-ignored; `agent/.env.example` lists the names with empty values. The repo is public.
- **Session:** context of the last exchanges kept **2 minutes**; undo covers the last action for
  **10 s** after it, per deck. Nothing persists between runs.
- Persona: Zero Two, cheerful and a little smug, **one short sentence at most**, a sprinkle of
  "hai~", never a joke when an action was asked.
- Commits: conventional (`type(scope): subject`), ending with the Co-Authored-By line.

## Review Focus

- **Empty or garbage text** ("", "uhh", music lyrics): no tool runs; reply is `nao_entendi`.
- **Spotify has no active device** (HTTP 404 `NO_ACTIVE_DEVICE`, the iPhone app closed): the tool
  returns an error the reply turns into "abre o Spotify no celular~", not a crash or a 500.
- **The Spotify access token expires mid-session** (401 after ~1 h): one refresh and one retry,
  then the error reply.
- **Undo with nothing to undo, or after 10 s:** reply "nada pra desfazer~", nothing changes.
- **Two commands from the same deck back to back** (the second arrives while the first runs):
  they are handled in arrival order per deck, so undo and context see them in that order.

Each line gets a test in the task that owns the code (marked "(Review Focus)").

---

## File structure

```
agent/
  package.json, tsconfig.json, vitest.config.ts, .env.example, README.md
  src/
    config.ts        env → typed Config (models, keys, port, limits)
    protocol.ts      CommandRequest / Reply / LightAction / StockId types
    lights.ts        LightAction → the yellow deck's /say query (zone + words ledvoice knows)
    session.ts       per-deck context (2 min) + undo snapshot (10 s) + per-deck queue
    spotify.ts       Spotify Web API client: refresh, play/search, control, volume, queue, save, now
    tools.ts         tool definitions (strict JSON Schema) + executeTool() + reply templates
    persona.ts       system prompts (fast, slow), stock phrase ids and their texts
    fastpath.ts      one Messages API call → tool → Reply; hands off to the slow path
    slowpath.ts      Agent SDK query() with the tools as an in-process MCP server
    trace.ts         one JSON line per command (stages, model, tokens, cost) + stats
    server.ts        POST /command, GET /health, GET /stats
    main.ts          wires real dependencies and starts the server
  evals/
    cases.jsonl      text → expected tool call (name + key args), or "ask" / "none" / "handoff"
    score.ts         pure scoring
    run.ts           runs cases through the fast path with a recording Spotify fake
  test/              vitest specs, one per src module
tools/spotify_auth.py   (+ --agent: agent scopes, writes agent/.env)
.gitignore           (+ agent/.env, agent/node_modules, agent/traces/)
```

---

### Task 1: Scaffold, protocol types and light actions

**Files:**
- Create: `agent/package.json`, `agent/tsconfig.json`, `agent/vitest.config.ts`,
  `agent/.env.example`, `agent/src/protocol.ts`, `agent/src/lights.ts`, `agent/test/lights.test.ts`
- Modify: `.gitignore`

**Interfaces:**
- Produces (`protocol.ts`):
  ```ts
  export type Zone = "inside" | "outside" | "both";
  export type Colour = "red" | "orange" | "pink" | "purple" | "blue" | "green" | "white" | "yellow";
  export type LightAction =
    | { kind: "set"; zone: Zone; colour?: Colour; level?: number; power?: "on" | "off" }
    | { kind: "step"; zone: Zone; direction: "up" | "down" }
    | { kind: "undo" };                       // the deck restores what it read before
  export type StockId = "ok" | "tocando" | "pulando" | "pausado" | "desfeito" | "nao_entendi"
    | "deixa_comigo" | "sem_spotify" | "nada_desfazer" | "erro";
  export interface CommandRequest { deck: string; text: string; }
  export interface Reply {
    heard: string; lights: LightAction[]; stock: StockId; say: string;
    mood: "happy" | "smug" | "talk"; path: "fast" | "slow" | "none"; traceId: string;
  }
  ```
- Produces (`lights.ts`): `toSayQuery(a: LightAction): { zone?: "inside" | "outside"; text: string } | null`
  — the words `src/util/ledvoice.h` understands: colour name in English, `on`/`off`, a level as
  `"<n>"` rounded to 10, `brighter`/`dimmer`; `zone` omitted for `both`; `null` for `undo` (the
  deck handles it).

- [ ] **Step 1: Scaffold.** `package.json` with `"type": "module"`, scripts `test` (`vitest run`),
  `start` (`tsx src/main.ts`), `eval` (`tsx evals/run.ts`); deps `@anthropic-ai/sdk`,
  `@anthropic-ai/claude-agent-sdk`, `zod`; dev deps `typescript`, `tsx`, `vitest`, `@types/node`
  (versions that support Node 18). `.env.example`: `ANTHROPIC_API_KEY=`, `FAST_MODEL=claude-haiku-5-5`,
  `SLOW_MODEL=claude-sonnet-5-5`, `SPOTIFY_CLIENT_ID=`, `SPOTIFY_CLIENT_SECRET=`,
  `SPOTIFY_REFRESH_TOKEN=`, `DECK_SECRET=`, `PORT=8787`. `.gitignore`: `agent/.env`,
  `agent/node_modules/`, `agent/traces/`.
- [ ] **Step 2: Failing tests** (`test/lights.test.ts`):
  ```ts
  expect(toSayQuery({ kind: "set", zone: "inside", colour: "blue", level: 33 }))
    .toEqual({ zone: "inside", text: "blue 30" });
  expect(toSayQuery({ kind: "set", zone: "outside", power: "off" })).toEqual({ zone: "outside", text: "off" });
  expect(toSayQuery({ kind: "set", zone: "both", colour: "purple" })).toEqual({ text: "purple" });
  expect(toSayQuery({ kind: "step", zone: "both", direction: "down" })).toEqual({ text: "dimmer" });
  expect(toSayQuery({ kind: "set", zone: "both", level: 100 })).toEqual({ text: "100" });
  expect(toSayQuery({ kind: "undo" })).toBeNull();
  ```
- [ ] **Step 3: Run** `cd agent && npm install && npm test` → fails (no module).
- [ ] **Step 4: Implement** `protocol.ts` and `lights.ts`. Level clamps to 0–100.
- [ ] **Step 5: Run** → pass.
- [ ] **Step 6: Commit** — `feat(agent): scaffold the voice agent service and light actions`

---

### Task 2: Session — context, undo, per-deck order

**Files:**
- Create: `agent/src/session.ts`, `agent/test/session.test.ts`

**Interfaces:**
- Produces:
  ```ts
  export interface Exchange { at: number; user: string; did: string; }   // did: "spotify_play Djavan"
  export type UndoFn = () => Promise<void>;
  export class Sessions {
    constructor(now: () => number = Date.now);
    context(deck: string): Exchange[];                 // exchanges younger than 2 min, oldest first
    remember(deck: string, e: Omit<Exchange, "at">): void;
    setUndo(deck: string, fn: UndoFn): void;           // replaces the previous one
    takeUndo(deck: string): UndoFn | null;             // only within 10 s; clears it
    run<T>(deck: string, job: () => Promise<T>): Promise<T>;   // per-deck FIFO
  }
  export const CONTEXT_MS = 120_000, UNDO_MS = 10_000;
  ```

- [ ] **Step 1: Failing tests:** context drops an exchange at 120 001 ms and keeps one at
  119 999 ms; `takeUndo` returns the fn at 9 999 ms, `null` at 10 001 ms, and `null` on a second
  take; decks don't see each other's context or undo; **(Review Focus)** `run` on one deck with a
  slow first job and a fast second job resolves in order `[1, 2]` (record the order both finish),
  while a job on another deck isn't held up by them.
- [ ] **Step 2: Run** `npm test -- session` → fails.
- [ ] **Step 3: Implement** (a promise chain per deck for `run`; injected clock for the windows).
- [ ] **Step 4: Run** → pass.
- [ ] **Step 5: Commit** — `feat(agent): per-deck session with context, undo and order`

---

### Task 3: Spotify client and the agent's token

**Files:**
- Create: `agent/src/spotify.ts`, `agent/test/spotify.test.ts`
- Modify: `tools/spotify_auth.py` (+ `--agent`)

**Interfaces:**
- Produces:
  ```ts
  export type Kind = "track" | "artist" | "album" | "playlist";
  export interface Playback { trackUri: string; contextUri: string | null; positionMs: number;
                              isPlaying: boolean; volume: number | null; title: string; artist: string; }
  export class SpotifyError extends Error { constructor(public code: "no_device" | "not_found" | "auth" | "other", msg: string); }
  export interface Spotify {
    now(): Promise<Playback | null>;
    play(query: string, kind: Kind): Promise<{ name: string }>;   // search, best match, start it
    control(action: "pause" | "resume" | "next" | "previous"): Promise<void>;
    volume(level: number): Promise<void>;                          // 0..100
    queue(query: string): Promise<{ name: string }>;
    save(playlist: string): Promise<{ name: string; playlist: string }>;  // current track
    restore(p: Playback): Promise<void>;                           // undo: context/track + position
  }
  export function createSpotify(cfg: { clientId: string; clientSecret: string; refreshToken: string },
                                fetchFn: typeof fetch = fetch): Spotify;
  ```
- Endpoints: token `POST https://accounts.spotify.com/api/token` (`grant_type=refresh_token`,
  Basic auth); `GET /v1/me/player`; `GET /v1/search?type=<kind>&limit=1&market=from_token`;
  `PUT /v1/me/player/play` (`context_uri` for artist/album/playlist, `uris` for a track);
  `PUT /v1/me/player/pause`; `POST /v1/me/player/next|previous`; `PUT /v1/me/player/volume?volume_percent=`;
  `POST /v1/me/player/queue?uri=`; `GET /v1/me/playlists` then `POST /v1/playlists/{id}/tracks`
  (create the playlist if no name matches, case-insensitive).
- `tools/spotify_auth.py --agent`: same flow with scopes
  `user-read-playback-state user-modify-playback-state user-read-currently-playing playlist-read-private playlist-modify-private playlist-modify-public`,
  writes `SPOTIFY_CLIENT_ID`, `SPOTIFY_CLIENT_SECRET`, `SPOTIFY_REFRESH_TOKEN` into `agent/.env`
  (creating it from `.env.example` if missing) and **never touches `src/config.h`**.

- [ ] **Step 1: Failing tests** with a fake `fetch` that records requests and answers from a table:
  `play("Djavan", "artist")` searches `type=artist` then `PUT /play` with that artist's
  `context_uri`; `play(…, "track")` sends `uris`; the first call fetches a token and later calls
  reuse it; **(Review Focus)** a 401 on `/me/player/next` triggers exactly one token refresh and one
  retry, and a second 401 throws `SpotifyError("auth")`; **(Review Focus)** a 404 with
  `reason: "NO_ACTIVE_DEVICE"` throws `SpotifyError("no_device")`; a search with no items throws
  `SpotifyError("not_found")`; `save("Carro")` with no playlist named "carro" creates it then adds
  the current track.
- [ ] **Step 2: Run** → fails.
- [ ] **Step 3: Implement** `spotify.ts`; then add `--agent` to `tools/spotify_auth.py` (keep the
  default behaviour byte-for-byte).
- [ ] **Step 4: Run** `npm test -- spotify` → pass; `python tools/spotify_auth.py --help` lists
  `--agent`.
- [ ] **Step 5: Commit** — `feat(agent): Spotify client with refresh and undo snapshot`

---

### Task 4: Persona, tools and replies

**Files:**
- Create: `agent/src/persona.ts`, `agent/src/tools.ts`, `agent/test/tools.test.ts`

**Interfaces:**
- Consumes: Task 1 types, Task 2 `Sessions`, Task 3 `Spotify`, `SpotifyError`.
- Produces (`persona.ts`): `FAST_SYSTEM: string`, `SLOW_SYSTEM: string`,
  `STOCK: Record<StockId, string>` (the line each stock phrase says, e.g. `tocando: "hai~ tocando"`).
  `FAST_SYSTEM` (pt-BR) says: pick exactly one tool for the command; colour words map to the eight
  colours; "dentro"/"fora" → inside/outside, neither → both; when the command is ambiguous (two
  artists that sound alike, an unclear target) call `ask`; anything needing history beyond the
  context given, or several steps, → `hand_off_to_agent`; "volta"/"desfaz" → `undo`; never answer
  in text instead of a tool. It ends with the persona rules from Global Constraints (used by `ask`).
- Produces (`tools.ts`):
  ```ts
  export const TOOL_DEFS: Anthropic.Tool[];   // lights, spotify_play, spotify_control,
      // spotify_volume, spotify_queue, spotify_save, spotify_now, ask, undo, hand_off_to_agent
  export interface ToolCtx { deck: string; spotify: Spotify; sessions: Sessions; }
  export interface ToolOutcome { lights: LightAction[]; stock: StockId; say: string;
                                 mood: Reply["mood"]; did: string; handoff?: string; }
      // mood: "smug" for ask and nada_desfazer, "happy" otherwise; the deck switches to "talk"
      // while her audio plays (part 3)
  export function executeTool(name: string, input: unknown, ctx: ToolCtx): Promise<ToolOutcome>;
  ```
  Schemas (all `strict: true`, `additionalProperties: false`, every property in `required`, optional
  ones as `["type", "null"]`): `lights {zone, colour|null, level|null, power|null, step|null}`;
  `spotify_play {query, kind}`; `spotify_control {action}`; `spotify_volume {level|null, delta|null}`;
  `spotify_queue {query}`; `spotify_save {playlist|null}` (null → "Carro"); `spotify_now {}`;
  `ask {question}`; `undo {}`; `hand_off_to_agent {task}`.
- Reply rules in `executeTool`: `spotify_play` → stock `tocando`, say `"hai~ tocando <name>"`;
  `control next` → `pulando`; `pause` → `pausado`; `lights` → `ok` (say from the action, e.g.
  `"hai~ azul dentro"`); `ask` → stock `ok`, say = the question, mood `smug`; `undo` → runs
  `sessions.takeUndo` → `desfeito`, or `nada_desfazer`; `hand_off_to_agent` → `deixa_comigo` +
  `handoff: task`; `SpotifyError("no_device")` → `sem_spotify` ("abre o Spotify no celular~");
  other errors → `erro`. Before a Spotify change, `executeTool` snapshots `spotify.now()` and
  `setUndo` restores it; a `lights` change sets an undo that returns `{kind: "undo"}` to the deck.

- [ ] **Step 1: Failing tests** (fake `Spotify` recording calls):
  `spotify_play {query:"Djavan",kind:"artist"}` → `say` "hai~ tocando Djavan", and a following
  `undo` within 10 s calls `restore` with the snapshot; `lights {zone:"inside",colour:"blue",level:30,…null}`
  → `lights: [{kind:"set",zone:"inside",colour:"blue",level:30}]`; `undo` after a lights change →
  `lights: [{kind:"undo"}]`; **(Review Focus)** `undo` with nothing pending → stock `nada_desfazer`
  and no Spotify call; **(Review Focus)** `no_device` → `sem_spotify`; `spotify_save {playlist:null}`
  saves to "Carro"; every `TOOL_DEFS` entry has `strict: true` and `additionalProperties: false`
  with all properties required; an unknown tool name → stock `erro`.
- [ ] **Step 2: Run** → fails.
- [ ] **Step 3: Implement** `persona.ts` and `tools.ts`.
- [ ] **Step 4: Run** `npm test -- tools` → pass.
- [ ] **Step 5: Commit** — `feat(agent): Zero Two's tools and replies`

---

### Task 5: Fast path

**Files:**
- Create: `agent/src/fastpath.ts`, `agent/test/fastpath.test.ts`

**Interfaces:**
- Consumes: Task 4 (`TOOL_DEFS`, `executeTool`, `FAST_SYSTEM`), Task 2 `Sessions`.
- Produces:
  ```ts
  export interface ModelClient { create(p: Anthropic.MessageCreateParamsNonStreaming): Promise<Anthropic.Message>; }
  export type SlowPath = (deck: string, task: string) => Promise<ToolOutcome>;
  export interface Stage { name: string; ms: number; }
  export interface FastResult { reply: Omit<Reply, "traceId">; stages: Stage[]; usage?: Anthropic.Usage; model: string; }
  export async function handleCommand(req: CommandRequest, deps: {
    model: ModelClient; modelId: string; ctx: Omit<ToolCtx, "deck">; slow: SlowPath; }): Promise<FastResult>;
  ```
- Request (Global Constraints): `model: modelId`, `system: [{type:"text", text: FAST_SYSTEM, cache_control:{type:"ephemeral"}}]`,
  `tools: TOOL_DEFS` (last one with `cache_control`), `thinking:{type:"disabled"}`,
  `output_config:{effort:"low"}`, `tool_choice:{type:"auto"}`, `max_tokens: 1024`, one user
  message: the session context as short lines (`"antes: <user> → <did>"`) then `"agora: <text>"`.
- Flow: trimmed text shorter than 2 letters → `nao_entendi` without a model call; otherwise one
  `create`; check `stop_reason` (`refusal`/`max_tokens` → `erro`); the **first** `tool_use` block
  runs through `executeTool`; none → `nao_entendi`; `handoff` set → `await slow(deck, task)` and
  reply with its outcome (path `slow`); `sessions.remember(...)` after any tool ran. The whole
  handler runs inside `sessions.run(deck, …)`. Stages: `model`, `tool`, and `slow` when used.

- [ ] **Step 1: Failing tests** (fake `ModelClient` returning canned `Message`s): a `tool_use`
  `spotify_play` → reply say "hai~ tocando Djavan", path `fast`, exactly **one** `create` call;
  the request carries `thinking.type "disabled"`, `output_config.effort "low"`, `tool_choice.type "auto"`,
  model id from deps, and the context lines from a previous command; **(Review Focus)** text `""` and
  `"uh"` → `nao_entendi` with **zero** `create` calls; a text-only answer (no `tool_use`) →
  `nao_entendi` and no tool ran; `stop_reason: "refusal"` → `erro`; `hand_off_to_agent` → `slow`
  called with the task, reply path `slow`.
- [ ] **Step 2: Run** → fails.
- [ ] **Step 3: Implement** `fastpath.ts`.
- [ ] **Step 4: Run** `npm test -- fastpath` → pass.
- [ ] **Step 5: Commit** — `feat(agent): one-call fast path that acts or hands off`

---

### Task 6: Slow path on the Claude Agent SDK

**Files:**
- Create: `agent/src/slowpath.ts`, `agent/test/slowpath.test.ts`

**Interfaces:**
- Consumes: Task 4 (`executeTool`, `TOOL_DEFS`'s names, `SLOW_SYSTEM`), `@anthropic-ai/claude-agent-sdk`
  (`query`, `tool`, `createSdkMcpServer`).
- Produces:
  ```ts
  export function carMcpTools(ctx: ToolCtx, out: ToolOutcome[]): SdkMcpToolDefinition<any>[];
      // each car tool except hand_off_to_agent, as an Agent SDK tool() with a zod shape matching
      // its JSON schema; the handler calls executeTool, pushes the outcome to `out`, and returns
      // its `did` (or the error text) as the tool's text result
  export function makeSlowPath(cfg: { modelId: string; ctx: Omit<ToolCtx, "deck">;
                                      runQuery?: typeof query }): SlowPath;
  ```
- `query` options: `model: modelId`, `systemPrompt: SLOW_SYSTEM`, `tools: []` (no built-in
  Bash/Read/Edit/Web tools), `mcpServers: { car: createSdkMcpServer({ name: "car", tools }) }`,
  `allowedTools` = every `mcp__car__<name>`, `permissionMode: "dontAsk"`, `settingSources: []`,
  `maxTurns: 8`, `maxBudgetUsd: 0.05`, `env: { ...process.env }`. The SlowPath result merges the
  collected outcomes: all `lights` concatenated, the last stock, and `say` = the final result text
  trimmed to one sentence (fallback to the last outcome's `say`). After **8 s** without a result it
  returns stock `deixa_comigo`, say "vai demorar um pouquinho~", and lets the query finish in the
  background (its outcomes still apply).
- No hooks in v1: no tool sends anything outside the car (spec: "confirm before sending" comes
  with the first such tool).

- [ ] **Step 1: Failing tests:** `carMcpTools` returns one tool per car tool except
  `hand_off_to_agent`, and calling a tool's handler with valid args runs `executeTool` (fake
  `Spotify`) and appends to `out`; `makeSlowPath` with an injected `runQuery` that yields a result
  message passes `tools: []`, `settingSources: []`, `permissionMode: "dontAsk"` and the
  `mcp__car__*` allow list, and maps the outcomes into one `ToolOutcome`.
- [ ] **Step 2: Run** → fails.
- [ ] **Step 3: Implement** `slowpath.ts` (read the SDK's `SDKResultMessage` type for the final
  text and `total_cost_usd`; don't guess field names).
- [ ] **Step 4: Run** `npm test -- slowpath` → pass. Then a **manual smoke run** with a real key and
  a Spotify fake: `npx tsx -e` calling the slow path with "põe uma do Djavan e depois aumenta o
  volume" logs two outcomes and a cost under US$0.05.
- [ ] **Step 5: Commit** — `feat(agent): Agent SDK slow path with the car tools`

---

### Task 7: Traces and the HTTP server

**Files:**
- Create: `agent/src/trace.ts`, `agent/src/server.ts`, `agent/src/config.ts`, `agent/src/main.ts`,
  `agent/test/trace.test.ts`, `agent/test/server.test.ts`

**Interfaces:**
- Produces (`trace.ts`):
  ```ts
  export interface Trace { id: string; at: string; deck: string; text: string; path: Reply["path"];
    stock: StockId; model: string; stages: Stage[]; totalMs: number;
    inputTokens: number; outputTokens: number; cacheReadTokens: number; costUsd: number; }
  export function costOf(model: string, usage: Anthropic.Usage): number;
      // Haiku 5.5 $0.10/$0.50 per MTok in/out, Sonnet 5.5 $2/$10 (cache reads $0.20); Haiku's
      // cache-read price from the pricing page (platform.claude.com/docs/en/about-claude/pricing),
      // not guessed; prices in one table so they're easy to update; unknown model → 0 + a warning
  export class TraceLog { constructor(dir: string); write(t: Trace): void;   // traces/YYYY-MM-DD.jsonl
    stats(days: number): { n: number; p50Ms: number; p95Ms: number; costUsd: number; byPath: Record<string, number> }; }
  ```
- Produces (`server.ts`): `createServer(deps): http.Server` with
  `POST /command` (JSON `CommandRequest`, header `Authorization: Bearer <DECK_SECRET>`) → `Reply`;
  `GET /health` → `{ok: true}` (no auth); `GET /stats?days=1` (auth) → `TraceLog.stats`.
  401 on a wrong or missing secret; 400 on a body that isn't `{deck: string, text: string}`; a
  thrown error → 200 with stock `erro` (the deck always gets a line to say) and the error in the trace.
- `config.ts`: reads `agent/.env` (a small parser, no dependency) and `process.env`; missing
  `ANTHROPIC_API_KEY` or `DECK_SECRET` stops the start with a clear message.

- [ ] **Step 1: Failing tests:** `costOf("claude-haiku-5-5", {input_tokens: 1_000_000, output_tokens: 0, …})`
  = 0.10; `stats` over three traces of 100, 200, 900 ms gives p50 200 and p95 900; the server on an
  ephemeral port: `/health` 200 without auth; `/command` without the secret 401; a bad body 400; a
  good one returns the fake handler's `Reply` with a `traceId` and writes one trace line; a handler
  that throws → 200 with stock `erro`.
- [ ] **Step 2: Run** → fails.
- [ ] **Step 3: Implement** the four files; `main.ts` builds the real `Anthropic` client
  (`new Anthropic()`), `createSpotify`, `Sessions`, `makeSlowPath`, and listens on `PORT`.
- [ ] **Step 4: Run** `npm test` (all) → pass; then `npm start` with a real `.env` and
  `curl -s -X POST localhost:8787/command -H "Authorization: Bearer $DECK_SECRET" -d '{"deck":"black","text":"toca Djavan"}'`
  plays Djavan on the phone and answers `"hai~ tocando Djavan"`.
- [ ] **Step 5: Commit** — `feat(agent): HTTP server with traces and stats`

---

### Task 8: Text eval set and the first baseline

**Files:**
- Create: `agent/evals/cases.jsonl`, `agent/evals/score.ts`, `agent/evals/run.ts`,
  `agent/test/score.test.ts`, `docs/voice-agent-evals.md`

**Interfaces:**
- Case line: `{"id": "p05", "text": "toca Djavan", "expect": {"tool": "spotify_play", "args": {"kind": "artist"}, "contains": {"query": "djavan"}}}`
  or `"expect": "ask" | "none" | "handoff"`. `contains` compares case- and accent-insensitive
  substrings; `args` compares exactly.
- Produces (`score.ts`):
  ```ts
  export type Verdict = "right" | "wrong_action" | "asked" | "missed";
      // wrong_action: a tool ran that isn't the expected one or with wrong args (the worst);
      // asked: ask/no-op when an action was expected; missed: no tool when ask/handoff expected
  export function judge(expect: Case["expect"], got: { tool: string | null; input: unknown }): Verdict;
  export function summarise(rows: { verdict: Verdict; ms: number; costUsd: number }[]):
      { accuracy: number; wrongActionRate: number; askRate: number; p50Ms: number; p95Ms: number; costPerCmd: number };
  ```
- `run.ts`: `npm run eval -- --model claude-haiku-5-5` runs every case through `handleCommand`
  with a recording Spotify fake and a slow-path stub (it only records the handoff), prints the
  summary and every non-`right` case, and appends a row to `docs/voice-agent-evals.md`
  (date, model, n, accuracy, wrong-action rate, ask rate, p50/p95, cost per command). Runs cost
  real credits: ~50 Haiku calls, a fraction of a cent.
- Cases (≥ 40): the ten mic-test phrases; colour/zone/level variants ("deixa fora vermelho no
  máximo", "dentro mais fraco"); Spotify variants ("pula", "volta a anterior", "pausa", "abaixa o
  som", "põe o álbum novo do Tyler, the Creator", "salva essa"); **ambiguous → ask**
  ("toca aquele cara", "deixa a luz bonita"); **multi-step → handoff** ("toca a que tocou antes do
  Djavan", "põe três músicas do Djavan na fila"); **garbage → none** ("", "é...", a line of lyrics);
  undo ("volta", "desfaz").

- [ ] **Step 1: Failing tests** for `judge` (each verdict, accent-insensitive `contains`) and
  `summarise` (rates and percentiles on a fixed list).
- [ ] **Step 2: Run** → fails.
- [ ] **Step 3: Implement** `score.ts`, `run.ts`, write `cases.jsonl`.
- [ ] **Step 4: Run** `npm test -- score` → pass. Then the **baseline** (needs the key):
  `npm run eval -- --model claude-haiku-5-5` and `-- --model claude-sonnet-5-5`; both rows land in
  `docs/voice-agent-evals.md`. Fix prompt or tool descriptions only if wrong-action rate > 0 on
  Haiku, re-run, and record each row (the table is the history).
- [ ] **Step 5: Commit** — `feat(agent): text eval set and first baselines`

---

### Task 9: Run docs

**Files:**
- Create: `agent/README.md`

- [ ] **Step 1:** Write how to run it: link the Max API credits (claude.ai → Settings → Billing →
  API credits), create the workspace with a spend limit and a key, `python tools/spotify_auth.py --agent`,
  fill `agent/.env`, `npm install`, `npm test`, `npm start`, the curl example, `npm run eval`, where
  traces are, `GET /stats`. Note what part 3 adds (audio, deck, deployment).
- [ ] **Step 2: Verify** the README's commands on a clean clone of the branch (`npm ci && npm test`).
- [ ] **Step 3: Commit** — `docs(agent): how to run the voice agent`
