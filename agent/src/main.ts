import type Anthropic from "@anthropic-ai/sdk";
import { fileURLToPath } from "node:url";
import { createAnthropic } from "./anthropic.js";
import { loadConfig, type Config } from "./config.js";
import { handleCommand } from "./fastpath.js";
import { createServer } from "./server.js";
import { Sessions } from "./session.js";
import { makeSlowPath } from "./slowpath.js";
import { createSpotify } from "./spotify.js";
import { TraceLog } from "./trace.js";

const agentDir = fileURLToPath(new URL("..", import.meta.url));

let cfg: Config;
try {
  cfg = loadConfig(`${agentDir}.env`);
} catch (e) {
  console.error(`voice-agent: ${e instanceof Error ? e.message : e}`);
  process.exit(1);
}

// The slow path (Agent SDK) reads the key from the environment.
process.env.ANTHROPIC_API_KEY = cfg.anthropicApiKey;

const client = createAnthropic(cfg.anthropicApiKey);
const traces = new TraceLog(`${agentDir}traces`);
const ctx = { spotify: createSpotify(cfg.spotify), sessions: new Sessions() };
const slow = makeSlowPath({
  modelId: cfg.slowModel, ctx,
  // A run that outlived its reply still spent credits: log them so /stats counts them.
  onLate: (l) => {
    try { traces.writeLate(l); } catch (e) { console.error("[trace] late write failed:", e); }
  },
});
const model = { create: (p: Anthropic.MessageCreateParamsNonStreaming) => client.messages.create(p) };

const server = createServer({
  secret: cfg.deckSecret,
  handle: (req) => handleCommand(req, { model, modelId: cfg.fastModel, ctx, slow }),
  traces,
});
server.listen(cfg.port, () => console.log(`voice-agent listening on :${cfg.port} (fast ${cfg.fastModel}, slow ${cfg.slowModel})`));
