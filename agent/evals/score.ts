// Scoring for the fast-path text evals. Pure: no I/O, no model.

export type Expect =
  | "ask" | "none" | "handoff"
  | { tool: string; args?: Record<string, unknown>; contains?: Record<string, string> };

/** One line of cases.jsonl. `before` texts run first in the same session (follow-ups, undo). */
export interface Case { id: string; text: string; before?: string[]; expect: Expect; }

export type Verdict = "right" | "wrong_action" | "asked" | "missed";
// wrong_action: a tool ran that isn't the expected one or with wrong args (the worst);
// asked: ask/no-op when an action was expected; missed: no tool when ask/handoff expected

export interface Got { tool: string | null; input: unknown; }

const fold = (s: string) => s.normalize("NFD").replace(/\p{M}/gu, "").toLowerCase();

function argsMatch(exp: Exclude<Expect, string>, input: unknown): boolean {
  const got = (input && typeof input === "object" ? input : {}) as Record<string, unknown>;
  for (const [k, v] of Object.entries(exp.args ?? {})) {
    if ((got[k] ?? null) !== v) return false;
  }
  for (const [k, v] of Object.entries(exp.contains ?? {})) {
    if (typeof got[k] !== "string" || !fold(got[k] as string).includes(fold(v))) return false;
  }
  return true;
}

export function judge(expect: Case["expect"], got: Got): Verdict {
  const tool = got.tool;
  if (expect === "none") {
    if (tool === null) return "right";
    return tool === "ask" ? "asked" : "wrong_action";
  }
  if (expect === "ask") {
    if (tool === "ask") return "right";
    return tool === null ? "missed" : "wrong_action";
  }
  if (expect === "handoff") {
    if (tool === "hand_off_to_agent") return "right";
    if (tool === null) return "missed";
    return tool === "ask" ? "asked" : "wrong_action";
  }
  if (tool === null || tool === "ask") return "asked";
  return tool === expect.tool && argsMatch(expect, got.input) ? "right" : "wrong_action";
}

// Nearest-rank, the same rule TraceLog.stats uses.
function percentile(sorted: number[], q: number): number {
  if (!sorted.length) return 0;
  return sorted[Math.min(sorted.length - 1, Math.max(0, Math.ceil(q * sorted.length) - 1))];
}

export function summarise(rows: { verdict: Verdict; ms: number; costUsd: number }[]):
    { accuracy: number; wrongActionRate: number; askRate: number; p50Ms: number; p95Ms: number; costPerCmd: number } {
  const n = rows.length;
  const rate = (v: Verdict) => (n ? rows.filter((r) => r.verdict === v).length / n : 0);
  const ms = rows.map((r) => r.ms).sort((a, b) => a - b);
  return {
    accuracy: rate("right"),
    wrongActionRate: rate("wrong_action"),
    askRate: rate("asked"),
    p50Ms: percentile(ms, 0.5),
    p95Ms: percentile(ms, 0.95),
    costPerCmd: n ? rows.reduce((s, r) => s + r.costUsd, 0) / n : 0,
  };
}

function validExpect(e: unknown): e is Expect {
  if (e === "ask" || e === "none" || e === "handoff") return true;
  if (!e || typeof e !== "object") return false;
  const o = e as Record<string, unknown>;
  const isObj = (v: unknown) => v === undefined || (typeof v === "object" && v !== null && !Array.isArray(v));
  return typeof o.tool === "string" && isObj(o.args) && isObj(o.contains);
}

/** Parses cases.jsonl: one JSON case per line, blank lines skipped. Throws on the first bad line. */
export function parseCases(text: string): Case[] {
  const cases: Case[] = [];
  const ids = new Set<string>();
  text.split(/\r?\n/).forEach((line, i) => {
    if (!line.trim()) return;
    const where = `cases.jsonl line ${i + 1}`;
    let c: Case;
    try { c = JSON.parse(line) as Case; } catch (e) { throw new Error(`${where}: ${e instanceof Error ? e.message : e}`); }
    const beforeOk = c.before === undefined || (Array.isArray(c.before) && c.before.every((b) => typeof b === "string"));
    if (typeof c.id !== "string" || typeof c.text !== "string" || !beforeOk || !validExpect(c.expect)) {
      throw new Error(`${where}: needs id, text, expect ("ask" | "none" | "handoff" | {tool, args?, contains?}) and an optional before list`);
    }
    if (ids.has(c.id)) throw new Error(`${where}: duplicate id ${c.id}`);
    ids.add(c.id);
    cases.push(c);
  });
  return cases;
}
