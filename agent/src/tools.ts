import type Anthropic from "@anthropic-ai/sdk";
import { STOCK } from "./persona.js";
import type { Colour, LightAction, Reply, StockId, Zone } from "./protocol.js";
import type { Sessions, UndoFn } from "./session.js";
import { SpotifyError, type Kind, type Playback, type Spotify } from "./spotify.js";

const COLOURS: Colour[] = ["red", "orange", "pink", "purple", "blue", "green", "white", "yellow"];
const DEFAULT_PLAYLIST = "Carro";

function tool(name: string, description: string, properties: Record<string, unknown>): Anthropic.Tool {
  return {
    name, description, strict: true,
    input_schema: { type: "object", properties, required: Object.keys(properties), additionalProperties: false },
  };
}

export const TOOL_DEFS: Anthropic.Tool[] = [
  tool("lights", "Muda as luzes do carro. Use só os campos pedidos; os outros vão null.", {
    zone: { type: "string", enum: ["inside", "outside", "both"], description: "dentro, fora ou as duas" },
    colour: { type: ["string", "null"], enum: [...COLOURS, null] },
    level: { type: ["integer", "null"], description: "brilho de 10 a 100; para apagar use power off" },
    power: { type: ["string", "null"], enum: ["on", "off", null], description: "acender ou apagar" },
    step: { type: ["string", "null"], enum: ["up", "down", null], description: "mais forte (up) ou mais fraco (down)" },
  }),
  tool("spotify_play", "Busca e toca no Spotify uma música, artista, álbum ou playlist.", {
    query: { type: "string", description: "o que buscar, como foi dito" },
    kind: { type: "string", enum: ["track", "artist", "album", "playlist"] },
  }),
  tool("spotify_control", "Pausa, continua, pula ou volta a música.", {
    action: { type: "string", enum: ["pause", "resume", "next", "previous"] },
  }),
  tool("spotify_volume", "Muda o volume do Spotify: level absoluto (0 a 100) ou delta relativo; o outro vai null.", {
    level: { type: ["integer", "null"] },
    delta: { type: ["integer", "null"], description: "ex.: +10 para mais alto, -10 para mais baixo" },
  }),
  tool("spotify_queue", "Põe uma música na fila, depois da atual.", {
    query: { type: "string", description: "a música a buscar" },
  }),
  tool("spotify_save", "Salva a música que está tocando numa playlist.", {
    playlist: { type: ["string", "null"], description: "nome da playlist; null para a playlist Carro" },
  }),
  tool("spotify_now", "Diz o que está tocando agora.", {}),
  tool("ask", "Faz uma pergunta curta quando o comando é ambíguo. Nada muda.", {
    question: { type: "string", description: "uma frase curta em português, no tom da Zero Two" },
  }),
  tool("undo", "Desfaz a última ação (luzes ou Spotify) dos últimos 10 segundos.", {}),
  tool("hand_off_to_agent", "Passa para o agente completo uma tarefa de vários passos ou que precisa de mais histórico.", {
    task: { type: "string", description: "a tarefa, em uma frase, com o que se sabe do contexto" },
  }),
];

export interface ToolCtx { deck: string; spotify: Spotify; sessions: Sessions; }
export interface ToolOutcome { lights: LightAction[]; stock: StockId; say: string;
                               mood: Reply["mood"]; did: string; handoff?: string; }

// Undo functions whose effect lives on the deck: running them is a no-op here,
// and the outcome tells the deck to restore what it read before.
const deckUndos = new WeakSet<UndoFn>();

const COLOUR_PT: Record<Colour, string> = {
  red: "vermelho", orange: "laranja", pink: "rosa", purple: "roxo",
  blue: "azul", green: "verde", white: "branco", yellow: "amarelo",
};
const ZONE_PT: Record<Zone, string> = { inside: "dentro", outside: "fora", both: "" };

function outcome(stock: StockId, did: string, extra: Partial<ToolOutcome> = {}): ToolOutcome {
  const mood = stock === "nada_desfazer" ? "smug" : "happy";
  return { lights: [], stock, say: STOCK[stock], mood, did, ...extra };
}

class BadInput extends Error {}

function str(v: unknown, field: string): string {
  if (typeof v !== "string" || v.trim() === "") throw new BadInput(`${field} missing`);
  return v.trim();
}
function oneOf<T extends string>(v: unknown, allowed: readonly T[], field: string): T {
  if (!allowed.includes(v as T)) throw new BadInput(`${field} invalid`);
  return v as T;
}
function optOneOf<T extends string>(v: unknown, allowed: readonly T[], field: string): T | undefined {
  return v == null ? undefined : oneOf(v, allowed, field);
}
function optInt(v: unknown, field: string): number | undefined {
  if (v == null) return undefined;
  if (typeof v !== "number" || !Number.isFinite(v)) throw new BadInput(`${field} invalid`);
  return Math.round(v);
}

function lightsTool(args: Record<string, unknown>, ctx: ToolCtx): ToolOutcome {
  const zone = oneOf(args.zone, ["inside", "outside", "both"] as const, "zone");
  const where = ZONE_PT[zone];
  const step = optOneOf(args.step, ["up", "down"] as const, "step");
  let action: LightAction;
  let words: string[];
  if (step) {
    action = { kind: "step", zone, direction: step };
    words = [step === "up" ? "mais forte" : "mais fraco"];
  } else {
    const colour = optOneOf(args.colour, COLOURS, "colour");
    const raw = optInt(args.level, "level");
    const level = raw === undefined ? undefined : Math.min(100, Math.max(10, raw)); // the deck floors at 10%
    const power = optOneOf(args.power, ["on", "off"] as const, "power");
    if (!colour && level === undefined && !power) throw new BadInput("nothing to change");
    action = { kind: "set", zone };
    words = [];
    if (colour) { action.colour = colour; words.push(COLOUR_PT[colour]); }
    if (level !== undefined) { action.level = level; words.push(`${level}%`); }
    if (power) { action.power = power; words.push(power === "on" ? "aceso" : "apagado"); }
  }
  const fn: UndoFn = async () => {};
  deckUndos.add(fn);
  ctx.sessions.setUndo(ctx.deck, fn);
  const say = ["hai~", ...words, where].filter(Boolean).join(" ");
  const did = ["lights", zone, ...words].join(" ");
  return outcome("ok", did, { lights: [action], say });
}

async function run(name: string, args: Record<string, unknown>, ctx: ToolCtx): Promise<ToolOutcome> {
  const { spotify, sessions, deck } = ctx;
  const setUndo = (fn: UndoFn) => sessions.setUndo(deck, fn);
  // Each Spotify change snapshots now() first; undo puts it back (nothing was playing → pause).
  const restoreTo = (snap: Playback | null): UndoFn =>
    snap ? () => spotify.restore(snap) : () => spotify.control("pause");

  switch (name) {
    case "lights":
      return lightsTool(args, ctx);

    case "spotify_play": {
      const query = str(args.query, "query");
      const kind = oneOf<Kind>(args.kind, ["track", "artist", "album", "playlist"], "kind");
      const snap = await spotify.now();
      const { name: played } = await spotify.play(query, kind);
      setUndo(restoreTo(snap));
      return outcome("tocando", `spotify_play ${played}`, { say: `${STOCK.tocando} ${played}` });
    }

    case "spotify_control": {
      const action = oneOf(args.action, ["pause", "resume", "next", "previous"] as const, "action");
      const snap = await spotify.now();
      await spotify.control(action);
      setUndo(restoreTo(snap));
      const did = `spotify_control ${action}`;
      if (action === "next") return outcome("pulando", did);
      if (action === "pause") return outcome("pausado", did);
      if (action === "resume") return outcome("tocando", did);
      return outcome("ok", did, { say: "hai~ voltando" });
    }

    case "spotify_volume": {
      const level = optInt(args.level, "level");
      const delta = optInt(args.delta, "delta");
      const snap = await spotify.now();
      const before = snap?.volume ?? null;
      let target: number;
      if (level !== undefined) target = level;
      else if (delta !== undefined && before !== null) target = before + delta;
      else throw new BadInput("no volume to set");
      target = Math.min(100, Math.max(0, target));
      await spotify.volume(target);
      // restore() does not reapply volume, and replaying the track would restart the song.
      if (before !== null) setUndo(() => spotify.volume(before));
      else sessions.takeUndo(deck); // old volume unknown: nothing to undo, and no older action either
      return outcome("ok", `spotify_volume ${target}`, { say: `hai~ volume ${target}` });
    }

    case "spotify_queue": {
      const { name: queued } = await spotify.queue(str(args.query, "query"));
      sessions.takeUndo(deck); // the queue cannot be undone; don't let undo hit an older action
      return outcome("ok", `spotify_queue ${queued}`, { say: `hai~ ${queued} na fila` });
    }

    case "spotify_save": {
      const playlist = args.playlist == null ? DEFAULT_PLAYLIST : str(args.playlist, "playlist");
      const saved = await spotify.save(playlist);
      sessions.takeUndo(deck); // not undoable either
      return outcome("ok", `spotify_save ${saved.name} → ${saved.playlist}`,
        { say: `hai~ salvei ${saved.name} na ${saved.playlist}` });
    }

    case "spotify_now": {
      const p = await spotify.now();
      if (!p) return outcome("ok", "spotify_now nada", { say: "nada tocando agora~" });
      return outcome("ok", `spotify_now ${p.title}`, { say: `${p.title}, de ${p.artist}` });
    }

    case "ask": {
      const question = str(args.question, "question");
      return { ...outcome("ok", `ask ${question}`, { say: question }), mood: "smug" };
    }

    case "undo": {
      const fn = sessions.takeUndo(deck);
      if (!fn) return outcome("nada_desfazer", "undo nada");
      await fn();
      return outcome("desfeito", "undo", { lights: deckUndos.has(fn) ? [{ kind: "undo" }] : [] });
    }

    case "hand_off_to_agent": {
      const task = str(args.task, "task");
      return outcome("deixa_comigo", `hand_off_to_agent ${task}`, { handoff: task });
    }

    default:
      throw new BadInput(`unknown tool ${name}`);
  }
}

export async function executeTool(name: string, input: unknown, ctx: ToolCtx): Promise<ToolOutcome> {
  const args = (input && typeof input === "object" ? input : {}) as Record<string, unknown>;
  try {
    return await run(name, args, ctx);
  } catch (err) {
    // A failed change may have half-happened; undo must not revert an older action instead.
    if (name.startsWith("spotify_") && name !== "spotify_now") ctx.sessions.takeUndo(ctx.deck);
    const msg = err instanceof Error ? err.message : String(err);
    if (err instanceof SpotifyError && err.code === "no_device") return outcome("sem_spotify", `${name} sem_spotify`);
    if (err instanceof SpotifyError && err.code === "not_found") {
      return outcome("erro", `${name} not_found`, { say: "não achei isso~" });
    }
    if (err instanceof BadInput && name === "lights") return outcome("nao_entendi", `lights ${msg}`);
    return outcome("erro", `${name} erro: ${msg}`);
  }
}
