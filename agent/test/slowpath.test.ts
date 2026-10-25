import type { McpSdkServerConfigWithInstance, Options, SDKMessage, query } from "@anthropic-ai/claude-agent-sdk";
import { Client } from "@modelcontextprotocol/sdk/client/index.js";
import { InMemoryTransport } from "@modelcontextprotocol/sdk/inMemory.js";
import { afterEach, describe, expect, it, vi } from "vitest";
import { SLOW_SYSTEM, STOCK } from "../src/persona.js";
import { Sessions } from "../src/session.js";
import { carMcpTools, makeSlowPath } from "../src/slowpath.js";
import { SpotifyError, type Spotify } from "../src/spotify.js";
import { TOOL_DEFS, type ToolOutcome } from "../src/tools.js";

function fakeSpotify() {
  const calls: unknown[][] = [];
  const spotify = {
    async now() { return { title: "Oceano", artist: "Djavan", volume: 40 }; },
    async play(q: string, k: string) { calls.push(["play", q, k]); return { name: q }; },
    async volume(v: number) { calls.push(["volume", v]); },
  } as unknown as Spotify;
  return { spotify, calls };
}

type Step = { tool: string; args: Record<string, unknown> } | SDKMessage;

// Plays the part of the Agent SDK: calls the car tools through the real in-process MCP server
// it was given, then yields the scripted messages.
function fakeRunQuery(steps: Step[], seen: { prompt?: unknown; options?: Options }) {
  return ((params: { prompt: unknown; options?: Options }) => {
    seen.prompt = params.prompt;
    seen.options = params.options;
    return (async function* () {
      const server = params.options!.mcpServers!.car as McpSdkServerConfigWithInstance;
      const [clientT, serverT] = InMemoryTransport.createLinkedPair();
      await server.instance.connect(serverT);
      const client = new Client({ name: "fake", version: "0" });
      await client.connect(clientT);
      try {
        for (const s of steps) {
          if ("tool" in s) await client.callTool({ name: s.tool, arguments: s.args });
          else yield s;
        }
      } finally { await client.close(); }
    })();
  }) as unknown as typeof query;
}

const result = (text: string, cost = 0.012): SDKMessage =>
  ({ type: "result", subtype: "success", result: text, total_cost_usd: cost, is_error: false }) as unknown as SDKMessage;

describe("carMcpTools", () => {
  it("offers every car tool except hand_off_to_agent", () => {
    const { spotify } = fakeSpotify();
    const tools = carMcpTools({ deck: "yellow", spotify, sessions: new Sessions() }, []);
    const want = TOOL_DEFS.map((t) => t.name).filter((n) => n !== "hand_off_to_agent");
    expect(tools.map((t) => t.name)).toEqual(want);
  });

  it("runs executeTool from the handler, records the outcome and returns its did", async () => {
    const { spotify, calls } = fakeSpotify();
    const out: ToolOutcome[] = [];
    const tools = carMcpTools({ deck: "yellow", spotify, sessions: new Sessions() }, out);
    const play = tools.find((t) => t.name === "spotify_play")!;
    const res = await play.handler({ query: "Djavan", kind: "artist" }, {});
    expect(calls).toEqual([["play", "Djavan", "artist"]]);
    expect(out.map((o) => o.did)).toEqual(["spotify_play Djavan"]);
    expect(res.content).toEqual([{ type: "text", text: "spotify_play Djavan" }]);
    expect(res.isError).toBeUndefined();
  });

  it("marks erro, sem_spotify and nao_entendi outcomes as tool errors", async () => {
    const spotify = {
      async now() { return null; },
      async play() { throw new SpotifyError("no_device", "no active Spotify device"); },
      async control() { throw new Error("boom"); },
    } as unknown as Spotify;
    const tools = carMcpTools({ deck: "yellow", spotify, sessions: new Sessions() }, []);
    const run = (name: string, args: Record<string, unknown>) => tools.find((t) => t.name === name)!.handler(args as never, {});
    expect((await run("spotify_play", { query: "Djavan", kind: "artist" })).isError).toBe(true);
    expect((await run("spotify_control", { action: "pause" })).isError).toBe(true);
    expect((await run("lights", { zone: "both", colour: null, level: null, power: null, step: null })).isError).toBe(true);
    expect((await run("undo", {})).isError).toBeUndefined();   // nada_desfazer is an answer, not a failure
  });
});

describe("makeSlowPath", () => {
  afterEach(() => { vi.useRealTimers(); });

  it("runs the query with only the car tools and merges the outcomes", async () => {
    const { spotify, calls } = fakeSpotify();
    const seen: { prompt?: unknown; options?: Options } = {};
    const runQuery = fakeRunQuery([
      { tool: "spotify_play", args: { query: "Djavan", kind: "artist" } },
      { tool: "lights", args: { zone: "inside", colour: "blue", level: null, power: null, step: null } },
      { tool: "spotify_volume", args: { level: null, delta: 10 } },
      result("Hai~ Djavan tocando e volume no 50. Mais alguma coisa?", 0.021),
    ], seen);
    const slow = makeSlowPath({ modelId: "slow-model", ctx: { spotify, sessions: new Sessions() }, runQuery });

    const res = await slow("yellow", "põe uma do Djavan e depois aumenta o volume");

    expect(seen.prompt).toBe("põe uma do Djavan e depois aumenta o volume");
    const o = seen.options!;
    expect(o.model).toBe("slow-model");
    expect(o.systemPrompt).toBe(SLOW_SYSTEM);
    expect(o.tools).toEqual([]);
    expect(o.settingSources).toEqual([]);
    expect(o.permissionMode).toBe("dontAsk");
    expect(o.maxTurns).toBe(8);
    expect(o.maxBudgetUsd).toBe(0.05);
    expect(o.env).toEqual({ ...process.env });
    expect(o.persistSession).toBe(false);
    expect(Object.keys(o.mcpServers!)).toEqual(["car"]);
    expect(o.allowedTools).toEqual(TOOL_DEFS.map((t) => t.name).filter((n) => n !== "hand_off_to_agent")
      .map((n) => `mcp__car__${n}`));

    expect(calls).toEqual([["play", "Djavan", "artist"], ["volume", 50]]);
    expect(res.lights).toEqual([{ kind: "set", zone: "inside", colour: "blue" }]);
    expect(res.stock).toBe("ok");                           // the last outcome's
    expect(res.say).toBe("hai~ volume 50");                // the last outcome's, never the prose
    expect(res.did).toBe("spotify_play Djavan; lights inside azul; spotify_volume 50");
    expect(res.costUsd).toBe(0.021);
  });

  it("falls back to the last outcome's say when the result has no text", async () => {
    const { spotify } = fakeSpotify();
    const runQuery = fakeRunQuery([
      { tool: "spotify_play", args: { query: "Djavan", kind: "artist" } },
      ({ type: "result", subtype: "error_max_turns", total_cost_usd: 0.04, is_error: true }) as unknown as SDKMessage,
    ], {});
    const slow = makeSlowPath({ modelId: "m", ctx: { spotify, sessions: new Sessions() }, runQuery });
    const res = await slow("yellow", "tarefa");
    expect(res.say).toBe("hai~ tocando Djavan");
    expect(res.stock).toBe("tocando");
    expect(res.costUsd).toBe(0.04);
  });

  it("says it failed when nothing ran and the query threw", async () => {
    const { spotify } = fakeSpotify();
    const runQuery = (() => (async function* () { throw new Error("boom"); })()) as unknown as typeof query;
    const spy = vi.spyOn(console, "error").mockImplementation(() => {});
    const slow = makeSlowPath({ modelId: "m", ctx: { spotify, sessions: new Sessions() }, runQuery });
    const res = await slow("yellow", "tarefa");
    expect(spy).toHaveBeenCalledTimes(1);
    spy.mockRestore();
    expect(res.stock).toBe("erro");
    expect(res.lights).toEqual([]);
  });

  it("answers deixa_comigo after 8 s and lets the query finish in the background", async () => {
    vi.useFakeTimers();
    const { spotify, calls } = fakeSpotify();
    let release!: () => void;
    const gate = new Promise<void>((r) => { release = r; });
    let finished = false;
    const runQuery = (() => (async function* () {
      await gate;
      await spotify.play("Djavan", "artist");
      finished = true;
      yield result("pronto.");
    })()) as unknown as typeof query;
    const slow = makeSlowPath({ modelId: "m", ctx: { spotify, sessions: new Sessions() }, runQuery });

    const pending = slow("yellow", "tarefa longa");
    await vi.advanceTimersByTimeAsync(7_999);
    let settled = false;
    void pending.then(() => { settled = true; });
    await Promise.resolve();
    expect(settled).toBe(false);
    await vi.advanceTimersByTimeAsync(1);
    const res = await pending;
    expect(res.stock).toBe("deixa_comigo");
    expect(res.say).toBe("vai demorar um pouquinho~");

    const info = vi.spyOn(console, "info").mockImplementation(() => {});
    vi.useRealTimers();
    release();
    await vi.waitFor(() => expect(info).toHaveBeenCalledTimes(1));
    info.mockRestore();
    expect(finished).toBe(true);
    expect(calls).toEqual([["play", "Djavan", "artist"]]);
  });

  it("says the question when the last outcome is ask", async () => {
    const { spotify } = fakeSpotify();
    const runQuery = fakeRunQuery([
      { tool: "spotify_play", args: { query: "Djavan", kind: "artist" } },
      { tool: "ask", args: { question: "qual do Djavan, Oceano ou Flor de Lis?" } },
      result("Perguntei qual música ele quer."),
    ], {});
    const slow = makeSlowPath({ modelId: "m", ctx: { spotify, sessions: new Sessions() }, runQuery });
    const res = await slow("yellow", "tarefa");
    expect(res.say).toBe("qual do Djavan, Oceano ou Flor de Lis?");
    expect(res.mood).toBe("smug");
  });

  it("late tool calls after the timeout leave the deck's undo alone, and the late finish logs its cost", async () => {
    vi.useFakeTimers();
    const infos: string[] = [];
    const spy = vi.spyOn(console, "info").mockImplementation((...a) => { infos.push(a.join(" ")); });
    try {
      const { spotify, calls } = fakeSpotify();
      const sessions = new Sessions();
      let release!: () => void;
      const gate = new Promise<void>((r) => { release = r; });
      const runQuery = ((params: { options?: Options }) => (async function* () {
        await gate;
        const server = params.options!.mcpServers!.car as McpSdkServerConfigWithInstance;
        const [clientT, serverT] = InMemoryTransport.createLinkedPair();
        await server.instance.connect(serverT);
        const client = new Client({ name: "fake", version: "0" });
        await client.connect(clientT);
        await client.callTool({ name: "spotify_volume", arguments: { level: 70, delta: null } });
        await client.callTool({ name: "spotify_queue", arguments: { query: "Flor de Lis" } });
        await client.close();
        yield result("pronto.", 0.033);
      })()) as unknown as typeof query;
      (spotify as unknown as { queue: (q: string) => Promise<{ name: string }> }).queue =
        async (q) => { calls.push(["queue", q]); return { name: q }; };
      const slow = makeSlowPath({ modelId: "m", ctx: { spotify, sessions }, runQuery });

      const pending = slow("yellow", "tarefa longa");
      await vi.advanceTimersByTimeAsync(8_000);
      expect((await pending).stock).toBe("deixa_comigo");

      const fastUndo = async () => {};
      sessions.setUndo("yellow", fastUndo);     // the driver's next fast-path action
      vi.useRealTimers();
      release();
      await vi.waitFor(() => expect(infos).toHaveLength(1));
      expect(calls).toEqual([["volume", 70], ["queue", "Flor de Lis"]]);
      expect(sessions.takeUndo("yellow")).toBe(fastUndo);
      expect(infos[0]).toContain("spotify_volume 70; spotify_queue Flor de Lis");
      expect(infos[0]).toContain("0.033");
    } finally {
      spy.mockRestore();
    }
  });

  it("a tool call that straddles the timeout leaves the deck's undo alone", async () => {
    vi.useFakeTimers();
    const info = vi.spyOn(console, "info").mockImplementation(() => {});
    try {
      const sessions = new Sessions();
      let release!: () => void;
      const gate = new Promise<void>((r) => { release = r; });
      let started = false, changed = false;
      const spotify = {
        async now() { return { title: "Oceano", artist: "Djavan", volume: 40 }; },
        async volume() { started = true; await gate; changed = true; },
      } as unknown as Spotify;
      const runQuery = fakeRunQuery([
        { tool: "spotify_volume", args: { level: 70, delta: null } },
        result("pronto.", 0.02),
      ], {});
      const slow = makeSlowPath({ modelId: "m", ctx: { spotify, sessions }, runQuery });

      const pending = slow("yellow", "tarefa longa");
      await vi.advanceTimersByTimeAsync(100);
      expect(started).toBe(true);                // the call began before the timeout
      await vi.advanceTimersByTimeAsync(7_900);
      expect((await pending).stock).toBe("deixa_comigo");

      const fastUndo = async () => {};
      sessions.setUndo("yellow", fastUndo);
      vi.useRealTimers();
      release();
      await vi.waitFor(() => expect(info).toHaveBeenCalledTimes(1));
      expect(changed).toBe(true);
      expect(sessions.takeUndo("yellow")).toBe(fastUndo);
    } finally {
      info.mockRestore();
    }
  });

  it("says the failure, not the model's prose, when an ask failed", async () => {
    const { spotify } = fakeSpotify();
    const runQuery = fakeRunQuery([
      { tool: "ask", args: { question: " " } },
      result("Não consegui perguntar."),
    ], {});
    const slow = makeSlowPath({ modelId: "m", ctx: { spotify, sessions: new Sessions() }, runQuery });
    const res = await slow("yellow", "tarefa");
    expect(res.stock).toBe("erro");
    expect(res.say).toBe(STOCK.erro);
  });

  it("says the failed outcome, not the model's claim, when the last tool failed", async () => {
    const spotify = {
      async now() { return null; },
      async play() { throw new SpotifyError("no_device", "no active Spotify device"); },
    } as unknown as Spotify;
    const runQuery = fakeRunQuery([
      { tool: "spotify_play", args: { query: "Djavan", kind: "artist" } },
      result("Pronto, Djavan tocando!"),
    ], {});
    const slow = makeSlowPath({ modelId: "m", ctx: { spotify, sessions: new Sessions() }, runQuery });
    const res = await slow("yellow", "toca Djavan");
    expect(res.stock).toBe("sem_spotify");
    expect(res.say).toBe(STOCK.sem_spotify);
  });

  it("says nao_entendi, not the model's claim, when no tool ran", async () => {
    const { spotify } = fakeSpotify();
    const runQuery = fakeRunQuery([result("Pronto, apaguei as luzes.")], {});
    const slow = makeSlowPath({ modelId: "m", ctx: { spotify, sessions: new Sessions() }, runQuery });
    const res = await slow("yellow", "apaga as luzes");
    expect(res.stock).toBe("nao_entendi");
    expect(res.say).toBe(STOCK.nao_entendi);
    expect(res.lights).toEqual([]);
    expect(res.did).toBe("slow nada");
  });

  it("(b) says nao_entendi when no tool ran, even if the model's claim ends in a question mark", async () => {
    const { spotify } = fakeSpotify();
    const runQuery = fakeRunQuery([result("Apaguei as luzes, tá?")], {});
    const slow = makeSlowPath({ modelId: "m", ctx: { spotify, sessions: new Sessions() }, runQuery });
    const res = await slow("yellow", "apaga as luzes");
    expect(res.stock).toBe("nao_entendi");
    expect(res.say).toBe(STOCK.nao_entendi);
    expect(res.lights).toEqual([]);
  });

  it("(a) says the first failed outcome when an earlier tool failed and the last worked", async () => {
    const calls: unknown[][] = [];
    const spotify = {
      async now() { return { title: "Oceano", artist: "Djavan", volume: 40 }; },
      async play() { throw new SpotifyError("no_device", "no active Spotify device"); },
      async volume(v: number) { calls.push(["volume", v]); },
    } as unknown as Spotify;
    const runQuery = fakeRunQuery([
      { tool: "spotify_play", args: { query: "Djavan", kind: "artist" } },
      { tool: "spotify_volume", args: { level: null, delta: 10 } },
      result("Pus Djavan e aumentei o volume"),
    ], {});
    const slow = makeSlowPath({ modelId: "m", ctx: { spotify, sessions: new Sessions() }, runQuery });
    const res = await slow("yellow", "põe Djavan e aumenta o volume");
    expect(calls).toEqual([["volume", 50]]);
    expect(res.stock).toBe("sem_spotify");
    expect(res.say).toBe(STOCK.sem_spotify);
    expect(res.did).toBe("spotify_play sem_spotify; spotify_volume 50");
  });

  it("(c) treats nothing to undo as a failure, not the model's 'desfeito!'", async () => {
    const { spotify } = fakeSpotify();
    const runQuery = fakeRunQuery([{ tool: "undo", args: {} }, result("desfeito!")], {});
    const slow = makeSlowPath({ modelId: "m", ctx: { spotify, sessions: new Sessions() }, runQuery });
    const res = await slow("yellow", "desfaz");
    expect(res.stock).toBe("nada_desfazer");
    expect(res.say).toBe(STOCK.nada_desfazer);
    expect(res.mood).toBe("smug");
  });

  it("(d) says the last outcome, not the prose, when every tool worked", async () => {
    const { spotify } = fakeSpotify();
    const runQuery = fakeRunQuery([
      { tool: "spotify_play", args: { query: "Djavan", kind: "artist" } },
      { tool: "spotify_volume", args: { level: null, delta: 10 } },
      result("Prontinho, tudo feito e mais um pouco!"),
    ], {});
    const slow = makeSlowPath({ modelId: "m", ctx: { spotify, sessions: new Sessions() }, runQuery });
    const res = await slow("yellow", "tarefa");
    expect(res.stock).toBe("ok");
    expect(res.say).toBe("hai~ volume 50");
  });

  it("reports the late finish's cost through onLate", async () => {
    vi.useFakeTimers();
    const info = vi.spyOn(console, "info").mockImplementation(() => {});
    try {
      const { spotify } = fakeSpotify();
      let release!: () => void;
      const gate = new Promise<void>((r) => { release = r; });
      const runQuery = fakeRunQuery([
        { tool: "spotify_play", args: { query: "Djavan", kind: "artist" } },
        result("pronto.", 0.044),
      ], {});
      const gated = ((p: unknown) => (async function* () { await gate; yield* (runQuery as any)(p); })()) as unknown as typeof query;
      const late: unknown[] = [];
      const slow = makeSlowPath({ modelId: "m", ctx: { spotify, sessions: new Sessions() }, runQuery: gated,
        onLate: (l) => { late.push(l); } });
      const pending = slow("yellow", "tarefa longa");
      await vi.advanceTimersByTimeAsync(8_000);
      expect((await pending).stock).toBe("deixa_comigo");
      vi.useRealTimers();
      release();
      await vi.waitFor(() => expect(late).toHaveLength(1));
      expect(late[0]).toEqual({ deck: "yellow", did: ["spotify_play Djavan"], costUsd: 0.044 });
    } finally {
      info.mockRestore();
    }
  });

  it("logs, not rejects, when the background query fails after the timeout", async () => {
    vi.useFakeTimers();
    const errors: unknown[] = [];
    const spy = vi.spyOn(console, "error").mockImplementation((...a) => { errors.push(a); });
    const unhandled: unknown[] = [];
    const onUnhandled = (e: unknown) => { unhandled.push(e); };
    process.on("unhandledRejection", onUnhandled);
    try {
      let fail!: () => void;
      const gate = new Promise<void>((r) => { fail = r; });
      const runQuery = (() => (async function* () {
        await gate;
        throw new Error("late boom");
      })()) as unknown as typeof query;
      const { spotify } = fakeSpotify();
      const slow = makeSlowPath({ modelId: "m", ctx: { spotify, sessions: new Sessions() }, runQuery });
      const pending = slow("yellow", "tarefa");
      await vi.advanceTimersByTimeAsync(8_000);
      expect((await pending).stock).toBe("deixa_comigo");
      vi.useRealTimers();
      fail();
      await vi.waitFor(() => expect(errors).toHaveLength(1));
      await new Promise((r) => setTimeout(r, 10));
      expect(unhandled).toEqual([]);
    } finally {
      process.off("unhandledRejection", onUnhandled);
      spy.mockRestore();
    }
  });
});
