import { createSdkMcpServer, query, tool, type SdkMcpToolDefinition } from "@anthropic-ai/claude-agent-sdk";
import { z } from "zod";
import type { SlowPath } from "./fastpath.js";
import { SLOW_SYSTEM, STOCK } from "./persona.js";
import type { StockId } from "./protocol.js";
import type { Sessions } from "./session.js";
import type { LateRun } from "./trace.js";
import { executeTool, TOOL_DEFS, type ToolCtx, type ToolOutcome } from "./tools.js";

const SERVER = "car";
const TIMEOUT_MS = 8_000;
const SLOW_SAY = "vai demorar um pouquinho~";

const COLOURS = ["red", "orange", "pink", "purple", "blue", "green", "white", "yellow"] as const;

// Zod shapes matching each TOOL_DEFS input_schema (nullable fields are required but may be null).
const SHAPES: Record<string, z.ZodRawShape> = {
  lights: {
    zone: z.enum(["inside", "outside", "both"]),
    colour: z.enum(COLOURS).nullable(),
    level: z.number().int().nullable(),
    power: z.enum(["on", "off"]).nullable(),
    step: z.enum(["up", "down"]).nullable(),
  },
  spotify_play: { query: z.string(), kind: z.enum(["track", "artist", "album", "playlist"]) },
  spotify_control: { action: z.enum(["pause", "resume", "next", "previous"]) },
  spotify_volume: { level: z.number().int().nullable(), delta: z.number().int().nullable() },
  spotify_queue: { query: z.string() },
  spotify_save: { playlist: z.string().nullable() },
  spotify_now: {},
  ask: { question: z.string() },
  undo: {},
};

const CAR_DEFS = TOOL_DEFS.filter((t) => t.name !== "hand_off_to_agent");

/** Outcomes where nothing (or not what was asked) happened: the model sees them as tool errors. */
const FAILED: ReadonlySet<StockId> = new Set<StockId>(["erro", "sem_spotify", "nao_entendi"]);

export function carMcpTools(ctx: ToolCtx, out: ToolOutcome[]): SdkMcpToolDefinition<any>[] {
  return CAR_DEFS.map((def) => {
    const shape = SHAPES[def.name];
    if (!shape) throw new Error(`no zod shape for ${def.name}`);
    return tool(def.name, def.description ?? "", shape, async (args) => {
      const o = await executeTool(def.name, args, ctx);
      out.push(o);
      return { content: [{ type: "text" as const, text: o.did }], ...(FAILED.has(o.stock) ? { isError: true } : {}) };
    });
  });
}

/** The last question in the text, or "" when it asks nothing. */
function lastQuestion(text: string): string {
  const qs = text.match(/[^.!?\n]*\?/g) ?? [];
  return (qs.at(-1) ?? "").trim();
}

function firstSentence(text: string): string {
  const t = text.trim();
  const m = /^.+?[.!?](?=\s|$)/s.exec(t);
  return (m ? m[0] : t.split("\n")[0]!).trim();
}

// The deck's sessions, with every undo write (set or clear) checked at the moment it happens:
// once off() is true they do nothing, even for a tool call that started earlier.
function undoUnless(sessions: Sessions, off: () => boolean): Sessions {
  const guarded = Object.create(sessions) as Sessions;
  guarded.setUndo = (deck, fn) => { if (!off()) sessions.setUndo(deck, fn); };
  guarded.takeUndo = (deck) => (off() ? null : sessions.takeUndo(deck));
  return guarded;
}

// The model's prose is only trusted after a tool that worked: it must never claim an action
// that failed or never ran.
function merge(outs: ToolOutcome[], text: string, costUsd?: number): ToolOutcome & { costUsd?: number } {
  const cost = costUsd === undefined ? {} : { costUsd };
  const last = outs.at(-1);
  if (!last) {
    // No tool ran. A question is fine to say (nothing claimed); anything else is not understood.
    const question = lastQuestion(text);
    if (question) return { lights: [], stock: "ok", say: question, mood: "smug", did: `slow ask ${question}`, ...cost };
    const stock: StockId = text.trim() ? "nao_entendi" : "erro";
    return { lights: [], stock, say: STOCK[stock], mood: "happy", did: "slow nada", ...cost };
  }
  const lights = outs.flatMap((o) => o.lights);
  const did = outs.map((o) => o.did).join("; ");
  if (FAILED.has(last.stock)) return { lights, stock: last.stock, say: last.say, mood: last.mood, did, ...cost };
  // A question is what she must say; the result text would contradict the ask.
  const asked = last.stock === "ok" && last.did.startsWith("ask ") ? last.say : "";
  const say = asked || firstSentence(text) || last.say;
  return { lights, stock: last.stock, say, mood: last.mood, did, ...cost };
}

export function makeSlowPath(cfg: {
  modelId: string; ctx: Omit<ToolCtx, "deck">; runQuery?: typeof query;
  /** Called when a run finishes after its reply went out (its cost was not in that command's trace). */
  onLate?: (l: LateRun) => void;
}): SlowPath {
  const runQuery = cfg.runQuery ?? query;
  const allowedTools = CAR_DEFS.map((t) => `mcp__${SERVER}__${t.name}`);

  return async (deck, task) => {
    const outs: ToolOutcome[] = [];
    // Once the reply has gone out, the deck has moved on: late tool calls must not touch its undo.
    let late = false;
    const ctx: ToolCtx = { ...cfg.ctx, deck, sessions: undoUnless(cfg.ctx.sessions, () => late) };
    const tools = carMcpTools(ctx, outs);

    const job = (async () => {
      let text = "";
      let costUsd: number | undefined;
      for await (const msg of runQuery({
        prompt: task,
        options: {
          model: cfg.modelId,
          systemPrompt: SLOW_SYSTEM,
          tools: [],
          mcpServers: { [SERVER]: createSdkMcpServer({ name: SERVER, tools }) },
          allowedTools,
          permissionMode: "dontAsk",
          settingSources: [],
          maxTurns: 8,
          maxBudgetUsd: 0.05,
          env: { ...process.env },
          persistSession: false, // the driver's words stay out of ~/.claude/projects
        },
      })) {
        if (msg.type !== "result") continue;
        costUsd = msg.total_cost_usd;
        if (msg.subtype === "success") text = msg.result;
      }
      const merged = merge(outs, text, costUsd);
      if (late) {
        console.info(`slow path finished late for ${deck}: ${merged.did}, US$${costUsd ?? "?"}`);
        try {
          cfg.onLate?.({ deck, did: outs.map((o) => o.did), costUsd: costUsd ?? 0 });
        } catch (e) { console.error("slow path onLate failed:", e); }
      }
      return merged;
    })();

    let timer: NodeJS.Timeout | undefined;
    const timeout = new Promise<"timeout">((resolve) => { timer = setTimeout(() => resolve("timeout"), TIMEOUT_MS); });
    // Never rejects: a failure (now or after the timeout) is logged and becomes what ran so far.
    const settled = job.catch((err: unknown) => {
      console.error(`slow path failed for ${deck}:`, err);
      return null;
    });
    try {
      const res = await Promise.race([settled, timeout]);
      if (res === "timeout") {
        late = true;
        // The query keeps going; its tools still change Spotify as they run.
        return { ...merge(outs, ""), stock: "deixa_comigo", say: SLOW_SAY, mood: "happy",
                 did: [...outs.map((o) => o.did), `slow em andamento: ${task}`].join("; ") };
      }
      return res ?? { ...merge(outs, ""), stock: "erro", say: STOCK.erro };
    } finally {
      clearTimeout(timer);
    }
  };
}
