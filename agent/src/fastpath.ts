import type Anthropic from "@anthropic-ai/sdk";
import { FAST_SYSTEM, STOCK } from "./persona.js";
import type { CommandRequest, Reply } from "./protocol.js";
import { executeTool, TOOL_DEFS, type ToolCtx, type ToolOutcome } from "./tools.js";

export interface ModelClient { create(p: Anthropic.MessageCreateParamsNonStreaming): Promise<Anthropic.Message>; }
export type SlowPath = (deck: string, task: string) => Promise<ToolOutcome & { costUsd?: number }>;
export interface Stage { name: string; ms: number; }
export interface FastResult {
  reply: Omit<Reply, "traceId">; stages: Stage[]; usage?: Anthropic.Usage; model: string; slowCostUsd?: number;
}

const MIN_LETTERS = 3;
const EPHEMERAL = { type: "ephemeral" } as const;

function reply(heard: string, o: Pick<ToolOutcome, "lights" | "stock" | "say" | "mood">, path: Reply["path"]): Omit<Reply, "traceId"> {
  return { heard, lights: o.lights, stock: o.stock, say: o.say, mood: o.mood, path };
}
const stockReply = (heard: string, stock: "nao_entendi" | "erro") =>
  reply(heard, { lights: [], stock, say: STOCK[stock], mood: "happy" }, "none");

export async function handleCommand(
  req: CommandRequest,
  deps: { model: ModelClient; modelId: string; ctx: Omit<ToolCtx, "deck">; slow: SlowPath },
): Promise<FastResult> {
  const { deck } = req;
  const text = req.text.trim();
  const { modelId } = deps;
  const stages: Stage[] = [];
  if ((text.match(/\p{L}/gu) ?? []).length < MIN_LETTERS) {
    return { reply: stockReply(text, "nao_entendi"), stages, model: modelId };
  }

  return deps.ctx.sessions.run(deck, async () => {
    const sessions = deps.ctx.sessions;
    const lines = sessions.context(deck).map((e) => `antes: ${e.user} → ${e.did}`);
    lines.push(`agora: ${text}`);
    const tools = TOOL_DEFS.map((t, i) => (i === TOOL_DEFS.length - 1 ? { ...t, cache_control: EPHEMERAL } : t));

    const timed = async <T>(name: string, job: () => Promise<T>): Promise<T> => {
      const start = Date.now();
      try { return await job(); } finally { stages.push({ name, ms: Date.now() - start }); }
    };

    const msg = await timed("model", () => deps.model.create({
      model: modelId,
      max_tokens: 1024,
      system: [{ type: "text", text: FAST_SYSTEM, cache_control: EPHEMERAL }],
      tools,
      tool_choice: { type: "auto" },
      thinking: { type: "disabled" },
      output_config: { effort: "low" },
      messages: [{ role: "user", content: lines.join("\n") }],
    }));
    const done = (r: Omit<Reply, "traceId">, extra: Partial<FastResult> = {}): FastResult =>
      ({ reply: r, stages, usage: msg.usage, model: modelId, ...extra });

    if (msg.stop_reason === "refusal" || msg.stop_reason === "max_tokens") return done(stockReply(text, "erro"));
    const use = msg.content.find((b): b is Anthropic.ToolUseBlock => b.type === "tool_use");
    if (!use) return done(stockReply(text, "nao_entendi"));

    const outcome = await timed("tool", () => executeTool(use.name, use.input, { ...deps.ctx, deck }));
    sessions.remember(deck, { user: text, did: outcome.did });
    if (!outcome.handoff) return done(reply(text, outcome, "fast"));

    const slow = await timed("slow", () => deps.slow(deck, outcome.handoff!));
    return done(reply(text, slow, "slow"), { slowCostUsd: slow.costUsd });
  });
}
