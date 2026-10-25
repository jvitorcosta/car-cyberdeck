import { describe, expect, it } from "vitest";
import { CONTEXT_MS, Sessions, UNDO_MS } from "../src/session.js";

function clock(start = 1_000_000) {
  let t = start;
  return { now: () => t, set: (v: number) => { t = v; } , t0: start };
}

describe("Sessions context", () => {
  it("keeps an exchange at 119 999 ms and drops it at 120 001 ms", () => {
    const c = clock();
    const s = new Sessions(c.now);
    s.remember("a", { user: "play Djavan", did: "spotify_play Djavan" });
    c.set(c.t0 + CONTEXT_MS - 1);
    expect(s.context("a")).toHaveLength(1);
    c.set(c.t0 + CONTEXT_MS + 1);
    expect(s.context("a")).toEqual([]);
  });
  it("returns exchanges oldest first with their timestamp", () => {
    const c = clock();
    const s = new Sessions(c.now);
    s.remember("a", { user: "one", did: "x" });
    c.set(c.t0 + 1000);
    s.remember("a", { user: "two", did: "y" });
    expect(s.context("a").map((e) => [e.user, e.at])).toEqual([["one", c.t0], ["two", c.t0 + 1000]]);
  });
  it("does not share context between decks", () => {
    const s = new Sessions();
    s.remember("a", { user: "u", did: "d" });
    expect(s.context("b")).toEqual([]);
  });
});

describe("Sessions undo", () => {
  it("returns the fn at 9 999 ms, null at 10 001 ms, and null on a second take", async () => {
    const c = clock();
    const s = new Sessions(c.now);
    let calls = 0;
    s.setUndo("a", async () => { calls++; });
    c.set(c.t0 + UNDO_MS - 1);
    const fn = s.takeUndo("a");
    expect(fn).not.toBeNull();
    await fn!();
    expect(calls).toBe(1);
    expect(s.takeUndo("a")).toBeNull();

    s.setUndo("a", async () => {});
    c.set(c.t0 + UNDO_MS - 1 + UNDO_MS + 1);
    expect(s.takeUndo("a")).toBeNull();
  });
  it("replaces the previous undo", async () => {
    const s = new Sessions();
    const hit: string[] = [];
    s.setUndo("a", async () => { hit.push("first"); });
    s.setUndo("a", async () => { hit.push("second"); });
    await s.takeUndo("a")!();
    expect(hit).toEqual(["second"]);
  });
  it("does not share undo between decks", () => {
    const s = new Sessions();
    s.setUndo("a", async () => {});
    expect(s.takeUndo("b")).toBeNull();
    expect(s.takeUndo("a")).not.toBeNull();
  });
});

describe("Sessions run", () => {
  const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));
  it("runs one deck's jobs in FIFO order without holding up another deck", async () => {
    const s = new Sessions();
    const done: string[] = [];
    const slow = s.run("a", async () => { await sleep(60); done.push("a1"); return 1; });
    const fast = s.run("a", async () => { done.push("a2"); return 2; });
    const other = s.run("b", async () => { done.push("b1"); return 3; });
    await other;
    expect(done).toEqual(["b1"]);
    expect(await Promise.all([slow, fast])).toEqual([1, 2]);
    expect(done).toEqual(["b1", "a1", "a2"]);
  });
  it("keeps running after a job rejects, and propagates the error to its caller", async () => {
    const s = new Sessions();
    const bad = s.run("a", async () => { throw new Error("boom"); });
    const good = s.run("a", async () => "ok");
    await expect(bad).rejects.toThrow("boom");
    await expect(good).resolves.toBe("ok");
  });
});
