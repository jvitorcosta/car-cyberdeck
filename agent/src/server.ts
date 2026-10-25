import { randomUUID, timingSafeEqual } from "node:crypto";
import http from "node:http";
import type { FastResult } from "./fastpath.js";
import { STOCK } from "./persona.js";
import type { CommandRequest, Reply } from "./protocol.js";
import { costOf, MAX_DAYS, type Trace, type TraceLog } from "./trace.js";

export interface ServerDeps {
  secret: string;
  handle: (req: CommandRequest) => Promise<FastResult>;
  traces: TraceLog;
}

const DECK_RE = /^[a-z0-9-]{1,32}$/;
const MAX_TEXT = 500;
const MAX_BODY = 8 * 1024;

function authorized(req: http.IncomingMessage, secret: string): boolean {
  const given = Buffer.from(req.headers.authorization ?? "");
  const want = Buffer.from(`Bearer ${secret}`);
  return given.length === want.length && timingSafeEqual(given, want);
}

function send(res: http.ServerResponse, status: number, body: unknown): void {
  const json = JSON.stringify(body);
  res.writeHead(status, { "Content-Type": "application/json", "Content-Length": Buffer.byteLength(json) });
  res.end(json);
}

function readBody(req: http.IncomingMessage): Promise<string | null> {
  return new Promise((resolve) => {
    let size = 0;
    const chunks: Buffer[] = [];
    req.on("data", (c: Buffer) => {
      size += c.length;
      if (size > MAX_BODY) { resolve(null); req.resume(); return; }
      chunks.push(c);
    });
    req.on("end", () => resolve(Buffer.concat(chunks).toString("utf8")));
    req.on("error", () => resolve(null));
    req.on("close", () => resolve(null));
  });
}

function parseCommand(raw: string | null): CommandRequest | null {
  if (raw === null) return null;
  let v: unknown;
  try { v = JSON.parse(raw); } catch { return null; }
  if (typeof v !== "object" || v === null) return null;
  const { deck, text } = v as Record<string, unknown>;
  if (typeof deck !== "string" || !DECK_RE.test(deck)) return null;
  if (typeof text !== "string" || text.length > MAX_TEXT) return null;
  return { deck, text };
}

async function command(cmd: CommandRequest, deps: ServerDeps): Promise<Reply> {
  const id = randomUUID();
  const start = Date.now();
  let result: FastResult | undefined;
  let error: string | undefined;
  try {
    result = await deps.handle(cmd);
  } catch (e) {
    error = e instanceof Error ? e.message : String(e);
  }
  const reply: Omit<Reply, "traceId"> = result?.reply ?? {
    heard: cmd.text, lights: [], stock: "erro", say: STOCK.erro, mood: "happy", path: "none",
  };
  const usage = result?.usage;
  const trace: Trace = {
    id, at: new Date(start).toISOString(), deck: cmd.deck, text: cmd.text,
    path: reply.path, stock: reply.stock, model: result?.model ?? "none",
    stages: result?.stages ?? [], totalMs: Date.now() - start,
    inputTokens: usage?.input_tokens ?? 0, outputTokens: usage?.output_tokens ?? 0,
    cacheReadTokens: usage?.cache_read_input_tokens ?? 0,
    costUsd: (result && usage ? costOf(result.model, usage) : 0) + (result?.slowCostUsd ?? 0),
    ...(error === undefined ? {} : { error }),
  };
  try { deps.traces.write(trace); } catch (e) { console.error("[trace] write failed:", e); }
  if (error !== undefined) console.error(`[command] ${id} failed: ${error}`);
  return { ...reply, traceId: id };
}

export function createServer(deps: ServerDeps): http.Server {
  return http.createServer(async (req, res) => {
    try {
      const url = new URL(req.url ?? "/", "http://localhost");
      if (req.method === "GET" && url.pathname === "/health") return send(res, 200, { ok: true });
      if (req.method === "GET" && url.pathname === "/stats") {
        if (!authorized(req, deps.secret)) return send(res, 401, { error: "unauthorized" });
        const days = Number(url.searchParams.get("days") ?? 1);
        return send(res, 200, deps.traces.stats(Number.isFinite(days) && days >= 1 ? Math.min(MAX_DAYS, Math.floor(days)) : 1));
      }
      if (req.method === "POST" && url.pathname === "/command") {
        if (!authorized(req, deps.secret)) return send(res, 401, { error: "unauthorized" });
        const cmd = parseCommand(await readBody(req));
        if (!cmd) return send(res, 400, { error: "expected {deck: [a-z0-9-]{1,32}, text: string <= 500 chars}" });
        return send(res, 200, await command(cmd, deps));
      }
      send(res, 404, { error: "not found" });
    } catch (e) {
      console.error("[server]", e);
      if (!res.headersSent) send(res, 500, { error: "internal" });
    }
  });
}
