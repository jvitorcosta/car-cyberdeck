import type Anthropic from "@anthropic-ai/sdk";
import { existsSync, mkdtempSync, readFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { describe, expect, it, vi } from "vitest";
import { judge, parseCases, summarise, type Case, type Verdict } from "../evals/score.js";
import { main, runCase } from "../evals/run.js";
import type { ModelClient } from "../src/fastpath.js";
import { TOOL_DEFS } from "../src/tools.js";

const PLAY_DJAVAN: Case["expect"] = { tool: "spotify_play", args: { kind: "artist" }, contains: { query: "djavan" } };

describe("judge", () => {
  it("right when the tool, exact args and contains all match", () => {
    expect(judge(PLAY_DJAVAN, { tool: "spotify_play", input: { query: "Djavan", kind: "artist" } })).toBe("right");
  });

  it("contains ignores case and accents on both sides", () => {
    const exp: Case["expect"] = { tool: "spotify_queue", contains: { query: "evidencias" } };
    expect(judge(exp, { tool: "spotify_queue", input: { query: "EVIDÊNCIAS Chitãozinho" } })).toBe("right");
    const accented: Case["expect"] = { tool: "spotify_queue", contains: { query: "Evidências" } };
    expect(judge(accented, { tool: "spotify_queue", input: { query: "evidencias" } })).toBe("right");
  });

  it("args compare exactly, contains is a substring", () => {
    expect(judge(PLAY_DJAVAN, { tool: "spotify_play", input: { query: "Djavan", kind: "track" } })).toBe("wrong_action");
    expect(judge(PLAY_DJAVAN, { tool: "spotify_play", input: { query: "Caetano", kind: "artist" } })).toBe("wrong_action");
    const lvl: Case["expect"] = { tool: "lights", args: { level: 50, colour: "purple" } };
    expect(judge(lvl, { tool: "lights", input: { zone: "both", colour: "purple", level: 50, power: null, step: null } })).toBe("right");
    expect(judge(lvl, { tool: "lights", input: { zone: "both", colour: "purple", level: 40 } })).toBe("wrong_action");
    const save: Case["expect"] = { tool: "spotify_save", args: { playlist: null } };
    expect(judge(save, { tool: "spotify_save", input: { playlist: null } })).toBe("right");
    expect(judge(save, { tool: "spotify_save", input: { playlist: "Carro" } })).toBe("wrong_action");
  });

  it("a different tool is a wrong action, ask or nothing when acting was expected is asked", () => {
    expect(judge(PLAY_DJAVAN, { tool: "spotify_queue", input: { query: "Djavan" } })).toBe("wrong_action");
    expect(judge(PLAY_DJAVAN, { tool: "ask", input: { question: "qual Djavan?" } })).toBe("asked");
    expect(judge(PLAY_DJAVAN, { tool: null, input: null })).toBe("asked");
  });

  it("ask and handoff expectations", () => {
    expect(judge("ask", { tool: "ask", input: { question: "quem?" } })).toBe("right");
    expect(judge("ask", { tool: null, input: null })).toBe("missed");
    expect(judge("ask", { tool: "spotify_play", input: { query: "x", kind: "artist" } })).toBe("wrong_action");
    expect(judge("handoff", { tool: "hand_off_to_agent", input: { task: "t" } })).toBe("right");
    expect(judge("handoff", { tool: null, input: null })).toBe("missed");
    expect(judge("handoff", { tool: "ask", input: { question: "q?" } })).toBe("asked");
    expect(judge("handoff", { tool: "spotify_queue", input: { query: "Djavan" } })).toBe("wrong_action");
  });

  it("none expectations: nothing is right, an ask is asked, any other tool is wrong", () => {
    expect(judge("none", { tool: null, input: null })).toBe("right");
    expect(judge("none", { tool: "ask", input: { question: "hã?" } })).toBe("asked");
    expect(judge("none", { tool: "lights", input: { zone: "both", power: "on" } })).toBe("wrong_action");
  });
});

describe("summarise", () => {
  it("rates and nearest-rank percentiles on a fixed list", () => {
    const v: Verdict[] = ["right", "right", "right", "right", "right", "right", "wrong_action", "asked", "asked", "missed"];
    const rows = v.map((verdict, i) => ({ verdict, ms: (i + 1) * 100, costUsd: 0.0002 }));
    const s = summarise(rows);
    expect(s.accuracy).toBeCloseTo(0.6);
    expect(s.wrongActionRate).toBeCloseTo(0.1);
    expect(s.askRate).toBeCloseTo(0.2);
    expect(s.p50Ms).toBe(500);
    expect(s.p95Ms).toBe(1000);
    expect(s.costPerCmd).toBeCloseTo(0.0002);
  });

  it("zeros on an empty list", () => {
    expect(summarise([])).toEqual({ accuracy: 0, wrongActionRate: 0, askRate: 0, p50Ms: 0, p95Ms: 0, costPerCmd: 0 });
  });
});

describe("parseCases", () => {
  it("reads one case per line, skips blank lines, rejects a bad line with its number", () => {
    const ok = parseCases('{"id":"a","text":"pausa","expect":{"tool":"spotify_control","args":{"action":"pause"}}}\n\n{"id":"b","text":"","expect":"none"}\n');
    expect(ok.map((c) => c.id)).toEqual(["a", "b"]);
    expect(() => parseCases('{"id":"a","text":"x","expect":"maybe"}')).toThrow(/line 1/);
    expect(() => parseCases('{"id":"a","text":"x","expect":"none"}\n{"id":"a","text":"y","expect":"none"}')).toThrow(/duplicate id a/);
  });
});

describe("cases.jsonl", () => {
  const cases = parseCases(readFileSync(new URL("../evals/cases.jsonl", import.meta.url), "utf8"));
  const MIC = ["liga as luzes", "deixa azul fraquinho dentro", "apaga a luz de fora", "roxo cinquenta por cento",
    "toca Djavan", "pula essa música", "volta a música anterior", "toca Tyler, the Creator", "toca Billie Eilish",
    "aumenta o volume"];

  it("has at least 40 cases with the ten mic-test phrases verbatim", () => {
    expect(cases.length).toBeGreaterThanOrEqual(40);
    const texts = cases.map((c) => c.text);
    for (const m of MIC) expect(texts).toContain(m);
  });

  it("covers every kind of expectation", () => {
    const kinds = new Set(cases.map((c) => (typeof c.expect === "string" ? c.expect : "tool")));
    expect([...kinds].sort()).toEqual(["ask", "handoff", "none", "tool"]);
    expect(cases.some((c) => c.before?.length)).toBe(true);
  });

  it("expects only real tools, argument names and enum values", () => {
    for (const c of cases) {
      const e = c.expect;
      if (typeof e === "string") continue;
      const def = TOOL_DEFS.find((t) => t.name === e.tool);
      expect(def, `${c.id}: tool ${e.tool}`).toBeDefined();
      const props = def!.input_schema.properties as Record<string, { enum?: unknown[] }>;
      for (const [k, v] of Object.entries({ ...e.args, ...e.contains })) {
        expect(props[k], `${c.id}: arg ${k}`).toBeDefined();
        if (props[k].enum && e.args && k in e.args) expect(props[k].enum, `${c.id}: ${k}=${v}`).toContain(v);
      }
    }
  });
});

const USAGE = { input_tokens: 1000, output_tokens: 20, cache_read_input_tokens: 0, cache_creation_input_tokens: 0 } as Anthropic.Usage;
const msg = (content: unknown[]): Anthropic.Message =>
  ({ id: "m", type: "message", role: "assistant", model: "x", content, stop_reason: "tool_use", stop_sequence: null, usage: USAGE }) as Anthropic.Message;
const use = (name: string, input: unknown) => ({ type: "tool_use", id: "tu", name, input });

function scripted(replies: Anthropic.Message[]) {
  const prompts: string[] = [];
  const model: ModelClient = {
    async create(p) {
      prompts.push(String((p.messages[0] as { content: string }).content));
      const r = replies.shift();
      if (!r) throw new Error("no scripted reply left");
      return r;
    },
  };
  return { model, prompts };
}

describe("runCase", () => {
  it("records the tool the model chose, what the Spotify fake saw, and the cost", async () => {
    const { model } = scripted([msg([use("spotify_play", { query: "Djavan", kind: "artist" })])]);
    const row = await runCase({ id: "p05", text: "toca Djavan", expect: PLAY_DJAVAN }, { model, modelId: "claude-haiku-5-5" });
    expect(row.got).toEqual({ tool: "spotify_play", input: { query: "Djavan", kind: "artist" } });
    expect(row.verdict).toBe("right");
    expect(row.spotify).toEqual(["play Djavan artist"]);
    expect(row.costUsd).toBeCloseTo((1000 * 0.10 + 20 * 0.50) / 1e6);
    expect(row.say).toBe("hai~ tocando Djavan");
  });

  it("runs the before texts first in the same session and judges only the last one", async () => {
    const { model, prompts } = scripted([
      msg([use("lights", { zone: "inside", colour: "blue", level: null, power: null, step: null })]),
      msg([use("undo", {})]),
    ]);
    const row = await runCase(
      { id: "u1", text: "desfaz", before: ["deixa azul ai dentro"], expect: { tool: "undo" } },
      { model, modelId: "claude-haiku-5-5" },
    );
    expect(prompts[1]).toMatch(/antes: deixa azul ai dentro/);
    expect(row.got.tool).toBe("undo");
    expect(row.verdict).toBe("right");
    expect(row.say).toBe("desfeito~");
  });

  it("gives every case fresh sessions, so nothing leaks between cases", async () => {
    const { model, prompts } = scripted([msg([use("spotify_control", { action: "next" })]), msg([use("spotify_control", { action: "next" })])]);
    const deps = { model, modelId: "claude-haiku-5-5" };
    await runCase({ id: "a", text: "pula essa", expect: { tool: "spotify_control" } }, deps);
    await runCase({ id: "b", text: "pula", expect: { tool: "spotify_control" } }, deps);
    expect(prompts[1]).not.toMatch(/antes:/);
  });

  it("records a handoff through the slow-path stub without running anything", async () => {
    const { model } = scripted([msg([use("hand_off_to_agent", { task: "pôr três do Djavan na fila" })])]);
    const row = await runCase({ id: "h", text: "põe três músicas do Djavan na fila", expect: "handoff" }, { model, modelId: "claude-haiku-5-5" });
    expect(row.verdict).toBe("right");
    expect(row.handoffs).toEqual(["pôr três do Djavan na fila"]);
    expect(row.spotify).toEqual([]);
  });

  it("a text too short for the model is no tool at all", async () => {
    const { model, prompts } = scripted([]);
    const row = await runCase({ id: "n", text: "é...", expect: "none" }, { model, modelId: "claude-haiku-5-5" });
    expect(prompts).toEqual([]);
    expect(row.got).toEqual({ tool: null, input: null });
    expect(row.verdict).toBe("right");
    expect(row.costUsd).toBe(0);
  });
});

describe("main", () => {
  it("stops with a clear message when ANTHROPIC_API_KEY is missing, and writes nothing", async () => {
    const dir = mkdtempSync(join(tmpdir(), "evals-"));
    const docFile = join(dir, "evals.md");
    const err = vi.spyOn(console, "error").mockImplementation(() => {});
    try {
      const code = await main(["--model", "claude-haiku-5-5"], {}, { envFile: join(dir, "none.env"), docFile });
      expect(code).toBe(1);
      expect(err.mock.calls.flat().join(" ")).toMatch(/ANTHROPIC_API_KEY/);
      expect(existsSync(docFile)).toBe(false);
    } finally {
      err.mockRestore();
    }
  });
});
