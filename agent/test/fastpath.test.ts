import type Anthropic from "@anthropic-ai/sdk";
import { describe, expect, it } from "vitest";
import { handleCommand, type ModelClient, type SlowPath } from "../src/fastpath.js";
import { Sessions } from "../src/session.js";
import type { Spotify } from "../src/spotify.js";
import { STOCK } from "../src/persona.js";

const USAGE = { input_tokens: 10, output_tokens: 5, cache_read_input_tokens: 0, cache_creation_input_tokens: 0 } as Anthropic.Usage;

function message(content: Anthropic.ContentBlock[], stop_reason: Anthropic.StopReason = "tool_use"): Anthropic.Message {
  return { id: "msg_1", type: "message", role: "assistant", model: "fake", content, stop_reason, stop_sequence: null, usage: USAGE } as Anthropic.Message;
}
const toolUse = (name: string, input: unknown): Anthropic.ContentBlock =>
  ({ type: "tool_use", id: "tu_1", name, input }) as Anthropic.ContentBlock;

function setup(reply: Anthropic.Message, slow?: SlowPath) {
  const requests: Anthropic.MessageCreateParamsNonStreaming[] = [];
  const model: ModelClient = { async create(p) { requests.push(p); return reply; } };
  const plays: unknown[][] = [];
  const spotify = {
    async now() { return null; },
    async play(q: string, k: string) { plays.push([q, k]); return { name: q }; },
  } as unknown as Spotify;
  const sessions = new Sessions();
  const deps = {
    model, modelId: "fake-model", ctx: { spotify, sessions },
    slow: slow ?? (async () => { throw new Error("slow not expected"); }),
  };
  return { deps, requests, sessions, plays };
}

const PLAY = toolUse("spotify_play", { query: "Djavan", kind: "artist" });

describe("handleCommand", () => {
  it("runs the first tool_use with exactly one model call", async () => {
    const { deps, requests, plays } = setup(message([PLAY]));
    const res = await handleCommand({ deck: "yellow", text: "toca Djavan" }, deps);
    expect(res.reply.say).toBe("hai~ tocando Djavan");
    expect(res.reply.path).toBe("fast");
    expect(res.reply.heard).toBe("toca Djavan");
    expect(requests).toHaveLength(1);
    expect(plays).toEqual([["Djavan", "artist"]]);
    expect(res.model).toBe("fake-model");
    expect(res.usage).toBe(USAGE);
    expect(res.stages.map((s) => s.name)).toEqual(["model", "tool"]);
  });

  it("sends the exact request shape with context lines from earlier commands", async () => {
    const { deps, requests, sessions } = setup(message([PLAY]));
    sessions.remember("yellow", { user: "acende a luz", did: "lights both aceso" });
    await handleCommand({ deck: "yellow", text: "toca Djavan" }, deps);
    const r = requests[0]!;
    expect(r.model).toBe("fake-model");
    expect(r.thinking).toEqual({ type: "disabled" });
    expect(r.output_config).toEqual({ effort: "low" });
    expect(r.tool_choice).toEqual({ type: "auto" });
    expect(r.max_tokens).toBe(1024);
    expect(r.system).toEqual([{ type: "text", text: expect.any(String), cache_control: { type: "ephemeral" } }]);
    const tools = r.tools as Anthropic.Tool[];
    expect(tools.at(-1)!.cache_control).toEqual({ type: "ephemeral" });
    expect(tools[0]!.cache_control).toBeUndefined();
    expect(r.messages).toEqual([{
      role: "user",
      content: "antes: acende a luz → lights both aceso\nagora: toca Djavan",
    }]);
  });

  it("remembers what it did for the next command", async () => {
    const { deps, sessions } = setup(message([PLAY]));
    await handleCommand({ deck: "yellow", text: "toca Djavan" }, deps);
    expect(sessions.context("yellow").map((e) => [e.user, e.did])).toEqual([["toca Djavan", "spotify_play Djavan"]]);
  });

  it.each(["", "  ", "uh"])("makes no model call for %j", async (text) => {
    const { deps, requests } = setup(message([PLAY]));
    const res = await handleCommand({ deck: "yellow", text }, deps);
    expect(res.reply.stock).toBe("nao_entendi");
    expect(res.reply.say).toBe(STOCK.nao_entendi);
    expect(requests).toHaveLength(0);
  });

  it("answers nao_entendi when the model only talks", async () => {
    const { deps, plays } = setup(message([{ type: "text", text: "oi", citations: null } as Anthropic.ContentBlock], "end_turn"));
    const res = await handleCommand({ deck: "yellow", text: "bla bla bla" }, deps);
    expect(res.reply.stock).toBe("nao_entendi");
    expect(plays).toHaveLength(0);
  });

  it.each(["refusal", "max_tokens"] as const)("answers erro on stop_reason %s", async (reason) => {
    const { deps, plays } = setup(message([PLAY], reason));
    const res = await handleCommand({ deck: "yellow", text: "toca Djavan" }, deps);
    expect(res.reply.stock).toBe("erro");
    expect(plays).toHaveLength(0);
  });

  it("hands off to the slow path and replies with its outcome", async () => {
    const calls: [string, string][] = [];
    const slow: SlowPath = async (deck, task) => {
      calls.push([deck, task]);
      return { lights: [], stock: "ok", say: "feito~", mood: "happy", did: "slow", costUsd: 0.03 };
    };
    const { deps } = setup(message([toolUse("hand_off_to_agent", { task: "toca o que eu curto" })]), slow);
    const res = await handleCommand({ deck: "yellow", text: "toca o que eu curto" }, deps);
    expect(calls).toEqual([["yellow", "toca o que eu curto"]]);
    expect(res.reply.path).toBe("slow");
    expect(res.reply.say).toBe("feito~");
    expect(res.slowCostUsd).toBe(0.03);
    expect(res.stages.map((s) => s.name)).toEqual(["model", "tool", "slow"]);
  });

  it("remembers what the slow path did, not just the handoff", async () => {
    const slow: SlowPath = async () =>
      ({ lights: [], stock: "ok", say: "feito~", mood: "happy", did: "spotify_play Djavan; spotify_volume 50" });
    const { deps, sessions } = setup(message([toolUse("hand_off_to_agent", { task: "Djavan e mais alto" })]), slow);
    await handleCommand({ deck: "yellow", text: "põe Djavan e aumenta" }, deps);
    expect(sessions.context("yellow").map((e) => [e.user, e.did]))
      .toEqual([["põe Djavan e aumenta", "spotify_play Djavan; spotify_volume 50"]]);
  });
  it("runs none of several tool_use blocks and hands the original text to the slow path", async () => {
    const calls: [string, string][] = [];
    const slow: SlowPath = async (deck, task) => {
      calls.push([deck, task]);
      return { lights: [], stock: "ok", say: "feito~", mood: "happy", did: "lights both apagado; spotify_control pause" };
    };
    const lightsOff = toolUse("lights", { zone: "both", colour: null, level: null, power: "off", step: null });
    const pause = { ...toolUse("spotify_control", { action: "pause" }), id: "tu_2" } as Anthropic.ContentBlock;
    const { deps, plays, sessions } = setup(message([lightsOff, pause]), slow);
    const res = await handleCommand({ deck: "yellow", text: "apaga a luz e pausa a música" }, deps);
    expect(calls).toEqual([["yellow", "apaga a luz e pausa a música"]]);
    expect(plays).toEqual([]);
    expect(res.reply.lights).toEqual([]);
    expect(res.reply.path).toBe("slow");
    expect(res.reply.say).toBe("feito~");
    expect(res.stages.map((s) => s.name)).toEqual(["model", "slow"]);
    expect(sessions.context("yellow").map((e) => e.did)).toEqual(["lights both apagado; spotify_control pause"]);
  });
});
