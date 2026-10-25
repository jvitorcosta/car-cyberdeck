import { existsSync, readFileSync } from "node:fs";

export interface Config {
  anthropicApiKey: string; deckSecret: string; port: number;
  fastModel: string; slowModel: string;
  spotify: { clientId: string; clientSecret: string; refreshToken: string };
}

/** Minimal KEY=value parser: `#` comments, optional quotes, no interpolation. */
export function parseEnv(text: string): Record<string, string> {
  const out: Record<string, string> = {};
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.trim();
    if (!line || line.startsWith("#")) continue;
    const eq = line.indexOf("=");
    if (eq < 1) continue;
    let v = line.slice(eq + 1).trim();
    if (v.length >= 2 && (v[0] === '"' || v[0] === "'") && v.endsWith(v[0])) v = v.slice(1, -1);
    out[line.slice(0, eq).trim()] = v;
  }
  return out;
}

/** The process environment wins; agent/.env fills in what it does not set. */
export function loadConfig(envFile: string, env: NodeJS.ProcessEnv = process.env): Config {
  const file = existsSync(envFile) ? parseEnv(readFileSync(envFile, "utf8")) : {};
  const get = (k: string) => (env[k] || file[k] || "").trim();
  const missing = ["ANTHROPIC_API_KEY", "DECK_SECRET"].filter((k) => !get(k));
  if (missing.length) {
    throw new Error(`missing ${missing.join(" / ")}: set ${missing.length > 1 ? "them" : "it"} in ${envFile} (see .env.example) or the environment`);
  }
  const port = Number(get("PORT") || 8787);
  if (!Number.isInteger(port) || port < 0 || port > 65535) throw new Error(`PORT must be 0-65535, got "${get("PORT")}"`);
  return {
    anthropicApiKey: get("ANTHROPIC_API_KEY"), deckSecret: get("DECK_SECRET"), port,
    fastModel: get("FAST_MODEL") || "claude-haiku-5-5",
    slowModel: get("SLOW_MODEL") || "claude-sonnet-5-5",
    spotify: { clientId: get("SPOTIFY_CLIENT_ID"), clientSecret: get("SPOTIFY_CLIENT_SECRET"), refreshToken: get("SPOTIFY_REFRESH_TOKEN") },
  };
}
