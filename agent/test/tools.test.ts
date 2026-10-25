import { describe, expect, it } from "vitest";
import { Sessions, UNDO_MS } from "../src/session.js";
import { SpotifyError, type Playback, type Spotify } from "../src/spotify.js";
import { STOCK } from "../src/persona.js";
import { executeTool, TOOL_DEFS, type ToolCtx } from "../src/tools.js";

const SNAPSHOT: Playback = {
  trackUri: "spotify:track:old", contextUri: "spotify:album:old", positionMs: 42_000,
  isPlaying: true, volume: 60, title: "Velha", artist: "Alguém",
};

function fakeSpotify(over: Partial<Spotify> = {}) {
  const calls: [string, ...unknown[]][] = [];
  const base: Spotify = {
    async now() { calls.push(["now"]); return SNAPSHOT; },
    async play(query, kind) { calls.push(["play", query, kind]); return { name: query }; },
    async control(action) { calls.push(["control", action]); },
    async volume(level) { calls.push(["volume", level]); },
    async queue(query) { calls.push(["queue", query]); return { name: query }; },
    async save(playlist) { calls.push(["save", playlist]); return { name: "Velha", playlist }; },
    async restore(p) { calls.push(["restore", p]); },
  };
  return { spotify: { ...base, ...over }, calls };
}

function setup(over: Partial<Spotify> = {}) {
  let t = 1_000_000;
  const clock = { now: () => t, advance: (ms: number) => { t += ms; } };
  const sessions = new Sessions(clock.now);
  const { spotify, calls } = fakeSpotify(over);
  const ctx: ToolCtx = { deck: "yellow", spotify, sessions };
  return { ctx, calls, clock };
}

const LIGHTS_NULLS = { colour: null, level: null, power: null, step: null };

describe("executeTool: spotify", () => {
  it("plays and says the name; undo within 10 s restores the snapshot", async () => {
    const { ctx, calls, clock } = setup();
    const out = await executeTool("spotify_play", { query: "Djavan", kind: "artist" }, ctx);
    expect(out).toMatchObject({ stock: "tocando", say: "hai~ tocando Djavan", mood: "happy", lights: [] });
    expect(calls).toEqual([["now"], ["play", "Djavan", "artist"]]);
    clock.advance(UNDO_MS - 1);
    const undo = await executeTool("undo", {}, ctx);
    expect(undo.stock).toBe("desfeito");
    expect(calls.at(-1)).toEqual(["restore", SNAPSHOT]);
  });

  it("next → pulando, pause → pausado", async () => {
    const { ctx } = setup();
    expect((await executeTool("spotify_control", { action: "next" }, ctx)).stock).toBe("pulando");
    expect((await executeTool("spotify_control", { action: "pause" }, ctx)).stock).toBe("pausado");
  });

  it("saves to Carro when the playlist is null", async () => {
    const { ctx, calls } = setup();
    const out = await executeTool("spotify_save", { playlist: null }, ctx);
    expect(calls).toContainEqual(["save", "Carro"]);
    expect(out.stock).toBe("ok");
  });

  it("undo of a volume change sets the old volume back (restore does not)", async () => {
    const { ctx, calls } = setup();
    await executeTool("spotify_volume", { level: null, delta: 10 }, ctx);
    expect(calls).toContainEqual(["volume", 70]);
    await executeTool("undo", {}, ctx);
    expect(calls.at(-1)).toEqual(["volume", 60]);
    expect(calls.some((c) => c[0] === "restore")).toBe(false);
  });

  it("absolute volume with an unknown current volume → a following undo answers nada_desfazer", async () => {
    const { ctx, calls } = setup({ async now() { return { ...SNAPSHOT, volume: null }; } });
    await executeTool("lights", { zone: "inside", ...LIGHTS_NULLS, colour: "red" }, ctx);
    expect((await executeTool("spotify_volume", { level: 40, delta: null }, ctx)).stock).toBe("ok");
    const undo = await executeTool("undo", {}, ctx);
    expect(undo).toMatchObject({ stock: "nada_desfazer", lights: [] });
    expect(calls.filter((c) => c[0] === "volume")).toEqual([["volume", 40]]);
  });

  it("a failed Spotify change clears the pending undo, so undo does not revert the earlier action", async () => {
    let fail = false;
    const { ctx, calls } = setup({
      async play(query, kind) {
        if (fail) throw new SpotifyError("no_device", "no active Spotify device");
        calls.push(["play", query, kind]);
        return { name: query };
      },
    });
    await executeTool("spotify_play", { query: "Djavan", kind: "artist" }, ctx);
    fail = true;
    expect((await executeTool("spotify_play", { query: "Djonga", kind: "artist" }, ctx)).stock).toBe("sem_spotify");
    const undo = await executeTool("undo", {}, ctx);
    expect(undo.stock).toBe("nada_desfazer");
    expect(calls.some((c) => c[0] === "restore")).toBe(false);
  });

  it("(Review Focus) no active device → sem_spotify with the phone line", async () => {
    const { ctx } = setup({
      async now() { throw new SpotifyError("no_device", "no active Spotify device"); },
    });
    const out = await executeTool("spotify_play", { query: "Djavan", kind: "artist" }, ctx);
    expect(out.stock).toBe("sem_spotify");
    expect(out.say).toBe("abre o Spotify no celular~");
    expect(out.lights).toEqual([]);
  });

  it("any other error → erro", async () => {
    const { ctx } = setup({ async play() { throw new Error("boom"); } });
    expect((await executeTool("spotify_play", { query: "x", kind: "track" }, ctx)).stock).toBe("erro");
  });
});

describe("executeTool: lights and undo", () => {
  it("maps a set to one light action and says it in Portuguese", async () => {
    const { ctx } = setup();
    const out = await executeTool("lights", { zone: "inside", ...LIGHTS_NULLS, colour: "blue", level: 30 }, ctx);
    expect(out.lights).toEqual([{ kind: "set", zone: "inside", colour: "blue", level: 30 }]);
    expect(out.stock).toBe("ok");
    expect(out.say).toContain("azul");
    expect(out.say).toContain("dentro");
  });

  it("maps a step to a step action", async () => {
    const { ctx } = setup();
    const out = await executeTool("lights", { zone: "both", ...LIGHTS_NULLS, step: "up" }, ctx);
    expect(out.lights).toEqual([{ kind: "step", zone: "both", direction: "up" }]);
  });

  it("undo after a lights change sends the deck an undo", async () => {
    const { ctx, calls } = setup();
    await executeTool("lights", { zone: "outside", ...LIGHTS_NULLS, power: "off" }, ctx);
    const out = await executeTool("undo", {}, ctx);
    expect(out).toMatchObject({ stock: "desfeito", lights: [{ kind: "undo" }] });
    expect(calls).toEqual([]);
  });

  it("(Review Focus) undo with nothing pending → nada_desfazer, no Spotify call", async () => {
    const { ctx, calls } = setup();
    const out = await executeTool("undo", {}, ctx);
    expect(out).toMatchObject({ stock: "nada_desfazer", say: STOCK.nada_desfazer, mood: "smug", lights: [] });
    expect(calls).toEqual([]);
  });

  it("(Review Focus) undo after 10 s → nada_desfazer, nothing restored", async () => {
    const { ctx, calls, clock } = setup();
    await executeTool("spotify_play", { query: "Djavan", kind: "artist" }, ctx);
    clock.advance(UNDO_MS);
    const out = await executeTool("undo", {}, ctx);
    expect(out.stock).toBe("nada_desfazer");
    expect(calls.some((c) => c[0] === "restore")).toBe(false);
  });
});

describe("executeTool: ask, hand-off, unknown", () => {
  it("ask says the question, smug", async () => {
    const { ctx } = setup();
    const out = await executeTool("ask", { question: "Djavan ou Djonga?" }, ctx);
    expect(out).toMatchObject({ stock: "ok", say: "Djavan ou Djonga?", mood: "smug" });
  });

  it("hand_off_to_agent → deixa_comigo with the task", async () => {
    const { ctx } = setup();
    const out = await executeTool("hand_off_to_agent", { task: "toca o que ouvi ontem" }, ctx);
    expect(out).toMatchObject({ stock: "deixa_comigo", handoff: "toca o que ouvi ontem" });
  });

  it("an unknown tool name → erro", async () => {
    const { ctx } = setup();
    expect((await executeTool("launch_rocket", {}, ctx)).stock).toBe("erro");
  });
});

describe("TOOL_DEFS", () => {
  it("has the ten tools", () => {
    expect(TOOL_DEFS.map((t) => t.name).sort()).toEqual([
      "ask", "hand_off_to_agent", "lights", "spotify_control", "spotify_now", "spotify_play",
      "spotify_queue", "spotify_save", "spotify_volume", "undo",
    ]);
  });

  it("every tool is strict, closed, and requires all its properties", () => {
    for (const t of TOOL_DEFS) {
      expect(t.strict, t.name).toBe(true);
      const s = t.input_schema as { additionalProperties?: boolean; properties?: object; required?: string[] };
      expect(s.additionalProperties, t.name).toBe(false);
      expect([...(s.required ?? [])].sort(), t.name).toEqual(Object.keys(s.properties ?? {}).sort());
    }
  });
});
