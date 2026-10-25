// Text evals for the fast path: every case in cases.jsonl through handleCommand with the real model,
// a recording Spotify fake and a slow-path stub. Usage: npm run eval -- --model claude-haiku-5-5
import Anthropic from "@anthropic-ai/sdk";
import { appendFileSync, existsSync, readFileSync } from "node:fs";
import { fileURLToPath, pathToFileURL } from "node:url";
import { parseEnv } from "../src/config.js";
import { handleCommand, type ModelClient, type SlowPath } from "../src/fastpath.js";
import { Sessions } from "../src/session.js";
import type { Playback, Spotify } from "../src/spotify.js";
import { costOf } from "../src/trace.js";
import { judge, parseCases, summarise, type Case, type Got, type Verdict } from "./score.js";

const DECK = "eval";
const CASES_FILE = fileURLToPath(new URL("./cases.jsonl", import.meta.url));
const ENV_FILE = fileURLToPath(new URL("../.env", import.meta.url));
const DOC_FILE = fileURLToPath(new URL("../../docs/voice-agent-evals.md", import.meta.url));

export interface Row {
  id: string; text: string; expect: Case["expect"]; got: Got; verdict: Verdict;
  ms: number; costUsd: number; say: string; spotify: string[]; handoffs: string[];
}

/** A Spotify that always has a song playing at volume 50 and records each call. */
function recordingSpotify(calls: string[]): Spotify {
  const playing: Playback = {
    trackUri: "spotify:track:eval", contextUri: null, positionMs: 30_000, isPlaying: true,
    volume: 50, title: "Oceano", artist: "Djavan",
  };
  return {
    async now() { return playing; },
    async play(query, kind) { calls.push(`play ${query} ${kind}`); return { name: query }; },
    async control(action) { calls.push(`control ${action}`); },
    async volume(level) { calls.push(`volume ${level}`); },
    async queue(query) { calls.push(`queue ${query}`); return { name: query }; },
    async save(playlist) { calls.push(`save ${playlist}`); return { name: playing.title, playlist }; },
    async restore(p) { calls.push(`restore ${p.trackUri}`); },
  };
}

/** Runs one case in fresh sessions: its `before` texts first, then the text that is judged. */
export async function runCase(c: Case, deps: { model: ModelClient; modelId: string }): Promise<Row> {
  const spotify: string[] = [];
  const handoffs: string[] = [];
  let last: Got | null = null;
  const model: ModelClient = {
    async create(p) {
      const m = await deps.model.create(p);
      const use = m.content.find((b): b is Anthropic.ToolUseBlock => b.type === "tool_use");
      last = use ? { tool: use.name, input: use.input } : { tool: null, input: null };
      return m;
    },
  };
  const slow: SlowPath = async (_deck, task) => {
    handoffs.push(task);
    return { lights: [], stock: "deixa_comigo", say: "deixa comigo~", mood: "happy", did: `slow ${task}` };
  };
  const cmdDeps = { model, modelId: deps.modelId, ctx: { spotify: recordingSpotify(spotify), sessions: new Sessions() }, slow };

  for (const text of c.before ?? []) await handleCommand({ deck: DECK, text }, cmdDeps);
  spotify.length = 0;
  handoffs.length = 0;
  last = null;

  const start = Date.now();
  const res = await handleCommand({ deck: DECK, text: c.text }, cmdDeps);
  const ms = Date.now() - start;
  const got: Got = last ?? { tool: null, input: null };
  return {
    id: c.id, text: c.text, expect: c.expect, got, verdict: judge(c.expect, got), ms,
    costUsd: res.usage ? costOf(deps.modelId, res.usage) : 0, say: res.reply.say, spotify, handoffs,
  };
}

const pct = (x: number) => `${(x * 100).toFixed(1)}%`;
const show = (e: Case["expect"] | Got) => JSON.stringify(e);

/** Returns the exit code. Only reads the key and calls the model once it has one. */
export async function main(
  argv: string[],
  env: NodeJS.ProcessEnv = process.env,
  opts: { envFile?: string; docFile?: string; casesFile?: string } = {},
): Promise<number> {
  const envFile = opts.envFile ?? ENV_FILE;
  const docFile = opts.docFile ?? DOC_FILE;
  const at = argv.indexOf("--model");
  const modelId = (at >= 0 ? argv[at + 1] : undefined) ?? "claude-haiku-5-5";

  const file = existsSync(envFile) ? parseEnv(readFileSync(envFile, "utf8")) : {};
  const apiKey = (env.ANTHROPIC_API_KEY || file.ANTHROPIC_API_KEY || "").trim();
  if (!apiKey) {
    console.error(`eval: ANTHROPIC_API_KEY is missing. Set it in ${envFile} or the environment; nothing was run.`);
    return 1;
  }

  const cases = parseCases(readFileSync(opts.casesFile ?? CASES_FILE, "utf8"));
  const client = new Anthropic({ apiKey });
  const model: ModelClient = { create: (p) => client.messages.create(p) };
  console.log(`eval: ${cases.length} cases on ${modelId}`);

  const rows: Row[] = [];
  for (const c of cases) {
    const row = await runCase(c, { model, modelId });
    rows.push(row);
    process.stdout.write(row.verdict === "right" ? "." : "x");
  }
  process.stdout.write("\n");

  const s = summarise(rows);
  console.log(`accuracy ${pct(s.accuracy)}  wrong action ${pct(s.wrongActionRate)}  asked ${pct(s.askRate)}  ` +
    `p50 ${s.p50Ms} ms  p95 ${s.p95Ms} ms  $${s.costPerCmd.toFixed(6)}/cmd`);
  for (const r of rows.filter((r) => r.verdict !== "right")) {
    console.log(`  ${r.verdict.padEnd(12)} ${r.id} "${r.text}"\n    expected ${show(r.expect)}\n    got      ${show(r.got)}  say "${r.say}"`);
  }

  const date = new Date().toISOString().slice(0, 10);
  appendFileSync(docFile, `| ${date} | ${modelId} | ${rows.length} | ${pct(s.accuracy)} | ${pct(s.wrongActionRate)} | ` +
    `${pct(s.askRate)} | ${s.p50Ms} / ${s.p95Ms} | $${s.costPerCmd.toFixed(6)} |\n`);
  console.log(`eval: row appended to ${docFile}`);
  return 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  main(process.argv.slice(2)).then(
    (code) => { process.exitCode = code; },
    (e) => { console.error(`eval: ${e instanceof Error ? e.message : e}`); process.exitCode = 1; },
  );
}
