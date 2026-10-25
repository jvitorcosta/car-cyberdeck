import type Anthropic from "@anthropic-ai/sdk";
import { appendFileSync, existsSync, mkdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import type { Reply, StockId } from "./protocol.js";
import type { Stage } from "./fastpath.js";

export interface Trace {
  id: string; at: string; deck: string; text: string; path: Reply["path"];
  stock: StockId; model: string; stages: Stage[]; totalMs: number;
  inputTokens: number; outputTokens: number; cacheReadTokens: number; costUsd: number;
  error?: string;
}

// USD per million tokens. The one place to update when prices change.
// Source: https://platform.claude.com/docs/en/about-claude/pricing (checked 2026-10-25;
// Haiku 5.5 rows are the "prompts up to 100,000 tokens" tier, which our short prompts always fit).
interface Price { input: number; output: number; cacheRead: number; cacheWrite: number; }
const PRICES: Record<string, Price> = {
  "claude-haiku-5-5": { input: 0.10, output: 0.50, cacheRead: 0.01, cacheWrite: 0.125 },
  "claude-sonnet-5-5": { input: 2, output: 10, cacheRead: 0.10, cacheWrite: 2.50 },
};

/** A slow-path run that finished after its command's reply went out: only its spend matters. */
export interface LateRun { deck: string; did: string[]; costUsd: number; }
export interface LateTrace extends LateRun { type: "late"; at: string; path: "slow"; }

const warned = new Set<string>();

export function costOf(model: string, usage: Anthropic.Usage): number {
  const p = PRICES[model];
  if (!p) {
    if (!warned.has(model)) { warned.add(model); console.warn(`[trace] no price for model "${model}", costing it at 0`); }
    return 0;
  }
  return (
    usage.input_tokens * p.input +
    usage.output_tokens * p.output +
    (usage.cache_read_input_tokens ?? 0) * p.cacheRead +
    (usage.cache_creation_input_tokens ?? 0) * p.cacheWrite
  ) / 1e6;
}

const DAY_MS = 86_400_000;
export const MAX_DAYS = 90;
const dayName = (ms: number) => new Date(ms).toISOString().slice(0, 10);

function percentile(sorted: number[], q: number): number {
  if (!sorted.length) return 0;
  return sorted[Math.min(sorted.length - 1, Math.max(0, Math.ceil(q * sorted.length) - 1))];
}

export class TraceLog {
  constructor(private dir: string, private now: () => number = Date.now) {}

  write(t: Trace): void {
    mkdirSync(this.dir, { recursive: true });
    appendFileSync(join(this.dir, `${dayName(this.now())}.jsonl`), JSON.stringify(t) + "\n");
  }

  /** Appends a late line: /stats adds its cost but does not count it as a command. */
  writeLate(l: LateRun): void {
    const line: LateTrace = { type: "late", at: new Date(this.now()).toISOString(), path: "slow", ...l };
    mkdirSync(this.dir, { recursive: true });
    appendFileSync(join(this.dir, `${dayName(this.now())}.jsonl`), JSON.stringify(line) + "\n");
  }

  stats(days: number): { n: number; p50Ms: number; p95Ms: number; costUsd: number; byPath: Record<string, number> } {
    const traces: Trace[] = [];
    let lateUsd = 0;
    const span = Math.min(MAX_DAYS, Math.max(1, Math.floor(Number(days)) || 1));
    for (let i = 0; i < span; i++) {
      const file = join(this.dir, `${dayName(this.now() - i * DAY_MS)}.jsonl`);
      if (!existsSync(file)) continue;
      for (const line of readFileSync(file, "utf8").split("\n")) {
        if (!line.trim()) continue;
        let t: Trace | LateTrace;
        try { t = JSON.parse(line) as Trace | LateTrace; } catch { continue; /* a torn line must not break stats */ }
        if ("type" in t && t.type === "late") lateUsd += Number(t.costUsd) || 0;
        else traces.push(t as Trace);
      }
    }
    const ms = traces.map((t) => t.totalMs).sort((a, b) => a - b);
    const byPath: Record<string, number> = {};
    for (const t of traces) byPath[t.path] = (byPath[t.path] ?? 0) + 1;
    return {
      n: traces.length,
      p50Ms: percentile(ms, 0.5),
      p95Ms: percentile(ms, 0.95),
      costUsd: traces.reduce((s, t) => s + t.costUsd, lateUsd),
      byPath,
    };
  }
}
