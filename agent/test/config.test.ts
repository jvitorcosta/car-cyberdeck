import { mkdtempSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { describe, expect, it } from "vitest";
import { loadConfig, parseEnv } from "../src/config.js";

describe("parseEnv", () => {
  it("strips an unquoted inline comment", () => {
    expect(parseEnv("DECK_SECRET=abc # deck\nPORT=8787   # port")).toEqual({ DECK_SECRET: "abc", PORT: "8787" });
  });

  it("keeps a # with no space before it, and everything inside quotes", () => {
    expect(parseEnv("A=abc#def")).toEqual({ A: "abc#def" });
    expect(parseEnv(`B="abc # not a comment" # comment\nC='x # y'`)).toEqual({ B: "abc # not a comment", C: "x # y" });
  });
});

describe("loadConfig", () => {
  const base = { ANTHROPIC_API_KEY: "k", DECK_SECRET: "s" };

  it("warns, but starts, when the Spotify values are missing", () => {
    const dir = mkdtempSync(join(tmpdir(), "cfg-"));
    writeFileSync(join(dir, ".env"), "SPOTIFY_CLIENT_ID=id\n");
    const warnings: string[] = [];
    const cfg = loadConfig(join(dir, ".env"), base, (m) => warnings.push(m));
    expect(cfg.deckSecret).toBe("s");
    expect(warnings).toHaveLength(1);
    expect(warnings[0]).toMatch(/SPOTIFY_CLIENT_SECRET/);
    expect(warnings[0]).toMatch(/SPOTIFY_REFRESH_TOKEN/);
    expect(warnings[0]).not.toMatch(/SPOTIFY_CLIENT_ID/);
  });

  it("says nothing when the Spotify values are all set", () => {
    const warnings: string[] = [];
    loadConfig("/nonexistent/.env", { ...base, SPOTIFY_CLIENT_ID: "a", SPOTIFY_CLIENT_SECRET: "b", SPOTIFY_REFRESH_TOKEN: "c" },
      (m) => warnings.push(m));
    expect(warnings).toEqual([]);
  });
});
