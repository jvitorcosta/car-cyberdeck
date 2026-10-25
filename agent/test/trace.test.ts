import { mkdtempSync, readFileSync, readdirSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { describe, expect, it } from "vitest";
import { costOf, TraceLog, type Trace } from "../src/trace.js";

const usage = (o: Partial<Record<string, number>>) =>
  ({ input_tokens: 0, output_tokens: 0, cache_creation_input_tokens: 0, cache_read_input_tokens: 0, ...o }) as never;

const trace = (totalMs: number, over: Partial<Trace> = {}): Trace => ({
  id: `t${totalMs}`, at: new Date().toISOString(), deck: "black", text: "toca", path: "fast", stock: "ok",
  model: "claude-haiku-5-5", stages: [], totalMs, inputTokens: 0, outputTokens: 0, cacheReadTokens: 0, costUsd: 0.5, ...over,
});

describe("costOf", () => {
  it("prices Haiku 5.5 input and output", () => {
    expect(costOf("claude-haiku-5-5", usage({ input_tokens: 1_000_000 }))).toBeCloseTo(0.1);
    expect(costOf("claude-haiku-5-5", usage({ output_tokens: 1_000_000 }))).toBeCloseTo(0.5);
  });
  it("prices cache reads below input", () => {
    expect(costOf("claude-haiku-5-5", usage({ cache_read_input_tokens: 1_000_000 }))).toBeCloseTo(0.01);
  });
  it("prices Sonnet 5.5", () => {
    expect(costOf("claude-sonnet-5-5", usage({ input_tokens: 1_000_000, output_tokens: 1_000_000 }))).toBeCloseTo(12);
  });
  it("unknown model costs 0", () => {
    expect(costOf("mystery", usage({ input_tokens: 1_000_000 }))).toBe(0);
  });
});

describe("TraceLog", () => {
  it("writes one jsonl line per trace in a dated file", () => {
    const dir = mkdtempSync(join(tmpdir(), "traces-"));
    new TraceLog(dir).write(trace(100));
    const files = readdirSync(dir);
    expect(files).toHaveLength(1);
    expect(files[0]).toMatch(/^\d{4}-\d{2}-\d{2}\.jsonl$/);
    expect(JSON.parse(readFileSync(join(dir, files[0]), "utf8").trim()).totalMs).toBe(100);
  });
  it("computes p50, p95, cost and per-path counts", () => {
    const log = new TraceLog(mkdtempSync(join(tmpdir(), "traces-")));
    log.write(trace(100)); log.write(trace(200)); log.write(trace(900, { path: "slow" }));
    expect(log.stats(1)).toEqual({ n: 3, p50Ms: 200, p95Ms: 900, costUsd: 1.5, byPath: { fast: 2, slow: 1 } });
  });
  it("is empty-safe", () => {
    expect(new TraceLog(join(tmpdir(), "no-such-traces-dir")).stats(1)).toEqual({ n: 0, p50Ms: 0, p95Ms: 0, costUsd: 0, byPath: {} });
  });
  it("clamps days to 90 so a huge value cannot loop forever", () => {
    const log = new TraceLog(mkdtempSync(join(tmpdir(), "traces-")));
    log.write(trace(100));
    const t0 = Date.now();
    expect(log.stats(1e12)).toEqual(log.stats(90));
    expect(Date.now() - t0).toBeLessThan(1000);
  });
});

describe("late slow-path finishes", () => {
  it("writes a late line whose cost /stats counts, without counting it as a command", () => {
    const dir = mkdtempSync(join(tmpdir(), "traces-"));
    const log = new TraceLog(dir);
    log.write(trace(100, { costUsd: 0.25 }));
    log.writeLate({ deck: "yellow", did: ["spotify_play Djavan", "spotify_volume 50"], costUsd: 0.04 });
    const [file] = readdirSync(dir);
    const late = JSON.parse(readFileSync(join(dir, file!), "utf8").trim().split("\n")[1]!);
    expect(late).toMatchObject({ type: "late", deck: "yellow", path: "slow", costUsd: 0.04,
      did: ["spotify_play Djavan", "spotify_volume 50"] });
    expect(typeof late.at).toBe("string");
    const s = log.stats(1);
    expect(s.n).toBe(1);
    expect(s.p50Ms).toBe(100);
    expect(s.byPath).toEqual({ fast: 1 });
    expect(s.costUsd).toBeCloseTo(0.29);
  });
});
