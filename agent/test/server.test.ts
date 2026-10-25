import { mkdtempSync, readFileSync, readdirSync } from "node:fs";
import type { AddressInfo } from "node:net";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { afterEach, describe, expect, it } from "vitest";
import type { FastResult } from "../src/fastpath.js";
import { createServer } from "../src/server.js";
import { TraceLog } from "../src/trace.js";

const SECRET = "s3cret";
const ok: FastResult = {
  reply: { heard: "toca Djavan", lights: [], stock: "tocando", say: "hai~ tocando Djavan", mood: "happy", path: "fast" },
  stages: [{ name: "model", ms: 5 }], model: "claude-haiku-5-5",
  usage: { input_tokens: 1000, output_tokens: 10, cache_read_input_tokens: 0, cache_creation_input_tokens: 0 } as never,
  slowCostUsd: 0.01,
};

let server: ReturnType<typeof createServer>;
afterEach(() => new Promise<void>((r) => { if (!server) return r(); server.close(() => r()); server.closeAllConnections(); }));

async function start(handle: () => Promise<FastResult>) {
  const dir = mkdtempSync(join(tmpdir(), "srv-"));
  const traces = new TraceLog(dir);
  server = createServer({ secret: SECRET, handle, traces });
  await new Promise<void>((r) => server.listen(0, "127.0.0.1", r));
  const base = `http://127.0.0.1:${(server.address() as AddressInfo).port}`;
  const post = (body: unknown, auth: string | null = `Bearer ${SECRET}`) =>
    fetch(`${base}/command`, {
      method: "POST", body: typeof body === "string" ? body : JSON.stringify(body),
      headers: auth ? { Authorization: auth } : {},
    });
  return { base, dir, post };
}

describe("server", () => {
  it("GET /health needs no auth", async () => {
    const { base } = await start(async () => ok);
    const r = await fetch(`${base}/health`);
    expect(r.status).toBe(200);
    expect(await r.json()).toEqual({ ok: true });
  });

  it("rejects a missing or wrong secret", async () => {
    const { post } = await start(async () => ok);
    expect((await post({ deck: "black", text: "toca" }, null)).status).toBe(401);
    expect((await post({ deck: "black", text: "toca" }, "Bearer nope")).status).toBe(401);
  });

  it.each([
    ["not json", "{oops"],
    ["no text", { deck: "black" }],
    ["text not a string", { deck: "black", text: 5 }],
    ["bad deck", { deck: "Black Deck!", text: "toca" }],
    ["text too long", { deck: "black", text: "a".repeat(501) }],
  ])("400 on %s", async (_n, body) => {
    const { post } = await start(async () => ok);
    expect((await post(body)).status).toBe(400);
  });

  it("returns the handler's reply with a traceId and writes one trace", async () => {
    const { post, dir } = await start(async () => ok);
    const r = await post({ deck: "black", text: "toca Djavan" });
    expect(r.status).toBe(200);
    const body = await r.json();
    expect(body).toMatchObject({ say: "hai~ tocando Djavan", stock: "tocando", path: "fast" });
    expect(typeof body.traceId).toBe("string");
    const lines = readFileSync(join(dir, readdirSync(dir)[0]), "utf8").trim().split("\n");
    expect(lines).toHaveLength(1);
    const t = JSON.parse(lines[0]);
    expect(t.id).toBe(body.traceId);
    expect(t.inputTokens).toBe(1000);
    expect(t.costUsd).toBeCloseTo(0.0001 + 0.000005 + 0.01);
  });

  it("a throwing handler still answers 200 with stock erro and logs the error", async () => {
    const { post, dir } = await start(async () => { throw new Error("boom"); });
    const r = await post({ deck: "black", text: "toca Djavan" });
    expect(r.status).toBe(200);
    expect(await r.json()).toMatchObject({ stock: "erro", path: "none" });
    expect(JSON.parse(readFileSync(join(dir, readdirSync(dir)[0]), "utf8").trim()).error).toBe("boom");
  });

  it("GET /stats needs auth and reports", async () => {
    const { base, post } = await start(async () => ok);
    await post({ deck: "black", text: "toca Djavan" });
    expect((await fetch(`${base}/stats?days=1`)).status).toBe(401);
    const r = await fetch(`${base}/stats?days=1`, { headers: { Authorization: `Bearer ${SECRET}` } });
    expect((await r.json()).n).toBe(1);
  });

  it("GET /stats clamps a huge days value", async () => {
    const { base, post } = await start(async () => ok);
    await post({ deck: "black", text: "toca Djavan" });
    const get = (q: string) => fetch(`${base}/stats?days=${q}`, { headers: { Authorization: `Bearer ${SECRET}` } }).then((r) => r.json());
    expect(await get("1e12")).toEqual(await get("90"));
  });
});
