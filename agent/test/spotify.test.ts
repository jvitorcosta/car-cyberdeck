import { describe, expect, it } from "vitest";
import { createSpotify, SpotifyError, type Playback } from "../src/spotify.js";

interface Req { method: string; url: URL; body: string | undefined; headers: Record<string, string>; }
type Reply = { status?: number; json?: unknown };
type Handler = (req: Req, n: number) => Reply;

// Fake fetch: records every request, answers from `routes` keyed "METHOD /path".
function fake(routes: Record<string, Handler | Reply>) {
  const calls: Req[] = [];
  const counts: Record<string, number> = {};
  const fetchFn = (async (input: unknown, init?: RequestInit) => {
    const url = new URL(String(input));
    const method = init?.method ?? "GET";
    const req: Req = {
      method, url, body: init?.body as string | undefined,
      headers: Object.fromEntries(Object.entries((init?.headers ?? {}) as Record<string, string>)),
    };
    calls.push(req);
    const key = `${method} ${url.pathname}`;
    counts[key] = (counts[key] ?? 0) + 1;
    const route = routes[key];
    if (!route) throw new Error(`unexpected request ${key}`);
    const r = typeof route === "function" ? route(req, counts[key]) : route;
    const status = r.status ?? 200;
    return new Response(status === 204 || r.json === undefined ? null : JSON.stringify(r.json), { status });
  }) as unknown as typeof fetch;
  return { calls, fetchFn, count: (key: string) => counts[key] ?? 0 };
}

const cfg = { clientId: "id", clientSecret: "secret", refreshToken: "refresh" };
const token = (n: number) => ({ json: { access_token: `tok${n}`, expires_in: 3600 } });
const tokenRoute: Handler = (_r, n) => token(n);

describe("token handling", () => {
  it("refreshes with Basic auth on the first call and reuses the token afterwards", async () => {
    const f = fake({
      "POST /api/token": tokenRoute,
      "PUT /v1/me/player/pause": { status: 204 },
    });
    const s = createSpotify(cfg, f.fetchFn);
    await s.control("pause");
    await s.control("pause");
    expect(f.count("POST /api/token")).toBe(1);
    const tokenReq = f.calls[0];
    expect(tokenReq.headers.Authorization).toBe("Basic " + Buffer.from("id:secret").toString("base64"));
    expect(tokenReq.body).toContain("grant_type=refresh_token");
    expect(tokenReq.body).toContain("refresh_token=refresh");
    expect(f.calls[1].headers.Authorization).toBe("Bearer tok1");
    expect(f.calls[2].headers.Authorization).toBe("Bearer tok1");
  });

  it("on a 401 refreshes exactly once and retries once", async () => {
    const f = fake({
      "POST /api/token": tokenRoute,
      "POST /v1/me/player/next": (_r, n) => (n === 1 ? { status: 401, json: { error: { status: 401 } } } : { status: 204 }),
    });
    const s = createSpotify(cfg, f.fetchFn);
    await s.control("next");
    expect(f.count("POST /api/token")).toBe(2);
    expect(f.count("POST /v1/me/player/next")).toBe(2);
    expect(f.calls.at(-1)!.headers.Authorization).toBe("Bearer tok2");
  });

  it("throws SpotifyError(auth) after a second 401, without looping", async () => {
    const f = fake({
      "POST /api/token": tokenRoute,
      "POST /v1/me/player/next": { status: 401, json: { error: { status: 401 } } },
    });
    const s = createSpotify(cfg, f.fetchFn);
    await expect(s.control("next")).rejects.toMatchObject({ name: "SpotifyError", code: "auth" });
    expect(f.count("POST /v1/me/player/next")).toBe(2);
    expect(f.count("POST /api/token")).toBe(2);
  });

  it("throws SpotifyError(auth) when the refresh itself is rejected", async () => {
    const f = fake({ "POST /api/token": { status: 400, json: { error: "invalid_grant" } } });
    const s = createSpotify(cfg, f.fetchFn);
    await expect(s.control("pause")).rejects.toMatchObject({ code: "auth" });
  });
});

describe("errors", () => {
  it("maps a 404 NO_ACTIVE_DEVICE to SpotifyError(no_device)", async () => {
    const f = fake({
      "POST /api/token": tokenRoute,
      "PUT /v1/me/player/pause": {
        status: 404,
        json: { error: { status: 404, message: "Player command failed: No active device found", reason: "NO_ACTIVE_DEVICE" } },
      },
    });
    const s = createSpotify(cfg, f.fetchFn);
    const err = await s.control("pause").catch((e) => e);
    expect(err).toBeInstanceOf(SpotifyError);
    expect(err.code).toBe("no_device");
  });

  it("throws SpotifyError(not_found) when a search has no items", async () => {
    const f = fake({
      "POST /api/token": tokenRoute,
      "GET /v1/search": { json: { artists: { items: [] } } },
    });
    const s = createSpotify(cfg, f.fetchFn);
    await expect(s.play("Nobody", "artist")).rejects.toMatchObject({ code: "not_found" });
  });

  it("maps other failures to SpotifyError(other)", async () => {
    const f = fake({
      "POST /api/token": tokenRoute,
      "PUT /v1/me/player/volume": { status: 500, json: { error: { status: 500, message: "boom" } } },
    });
    const s = createSpotify(cfg, f.fetchFn);
    await expect(s.volume(50)).rejects.toMatchObject({ code: "other" });
  });
});

describe("playback", () => {
  it("plays an artist by searching then sending its context_uri", async () => {
    const f = fake({
      "POST /api/token": tokenRoute,
      "GET /v1/search": { json: { artists: { items: [{ name: "Djavan", uri: "spotify:artist:1" }] } } },
      "PUT /v1/me/player/play": { status: 204 },
    });
    const s = createSpotify(cfg, f.fetchFn);
    expect(await s.play("Djavan", "artist")).toEqual({ name: "Djavan" });
    const search = f.calls.find((c) => c.url.pathname === "/v1/search")!;
    expect(search.url.searchParams.get("type")).toBe("artist");
    expect(search.url.searchParams.get("q")).toBe("Djavan");
    expect(search.url.searchParams.get("limit")).toBe("1");
    expect(search.url.searchParams.get("market")).toBe("from_token");
    const play = f.calls.find((c) => c.url.pathname === "/v1/me/player/play")!;
    expect(JSON.parse(play.body!)).toEqual({ context_uri: "spotify:artist:1" });
  });

  it("plays a track with uris", async () => {
    const f = fake({
      "POST /api/token": tokenRoute,
      "GET /v1/search": { json: { tracks: { items: [{ name: "Oceano", uri: "spotify:track:9", artists: [{ name: "Djavan" }] }] } } },
      "PUT /v1/me/player/play": { status: 204 },
    });
    const s = createSpotify(cfg, f.fetchFn);
    expect(await s.play("Oceano", "track")).toEqual({ name: "Oceano" });
    const play = f.calls.find((c) => c.url.pathname === "/v1/me/player/play")!;
    expect(JSON.parse(play.body!)).toEqual({ uris: ["spotify:track:9"] });
  });

  it("controls, volume and queue hit the right endpoints", async () => {
    const f = fake({
      "POST /api/token": tokenRoute,
      "PUT /v1/me/player/pause": { status: 204 },
      "PUT /v1/me/player/play": { status: 204 },
      "POST /v1/me/player/previous": { status: 204 },
      "PUT /v1/me/player/volume": { status: 204 },
      "GET /v1/search": { json: { tracks: { items: [{ name: "Oceano", uri: "spotify:track:9" }] } } },
      "POST /v1/me/player/queue": { status: 204 },
    });
    const s = createSpotify(cfg, f.fetchFn);
    await s.control("pause");
    await s.control("resume");
    await s.control("previous");
    await s.volume(142.4);
    expect(await s.queue("Oceano")).toEqual({ name: "Oceano" });
    const by = (m: string, p: string) => f.calls.find((c) => c.method === m && c.url.pathname === p)!;
    expect(by("PUT", "/v1/me/player/play").body).toBeUndefined();
    expect(by("PUT", "/v1/me/player/volume").url.searchParams.get("volume_percent")).toBe("100");
    expect(by("POST", "/v1/me/player/queue").url.searchParams.get("uri")).toBe("spotify:track:9");
    expect(f.calls.find((c) => c.url.pathname === "/v1/search")!.url.searchParams.get("type")).toBe("track");
  });

  it("now() maps the player state, and returns null when nothing plays", async () => {
    const state = {
      is_playing: true, progress_ms: 4200,
      device: { volume_percent: 35 },
      context: { uri: "spotify:album:7" },
      item: { uri: "spotify:track:9", name: "Oceano", artists: [{ name: "Djavan" }, { name: "X" }] },
    };
    let reply: Reply = { json: state };
    const f = fake({ "POST /api/token": tokenRoute, "GET /v1/me/player": () => reply });
    const s = createSpotify(cfg, f.fetchFn);
    expect(await s.now()).toEqual({
      trackUri: "spotify:track:9", contextUri: "spotify:album:7", positionMs: 4200,
      isPlaying: true, volume: 35, title: "Oceano", artist: "Djavan, X",
    });
    reply = { status: 204 };
    expect(await s.now()).toBeNull();
  });
});

describe("save", () => {
  const playing = {
    is_playing: true, progress_ms: 1, device: { volume_percent: 10 }, context: null,
    item: { uri: "spotify:track:9", name: "Oceano", artists: [{ name: "Djavan" }] },
  };

  it("creates the playlist when no name matches, then adds the current track", async () => {
    const f = fake({
      "POST /api/token": tokenRoute,
      "GET /v1/me/player": { json: playing },
      "GET /v1/me/playlists": { json: { items: [{ id: "p1", name: "Treino" }], next: null } },
      "POST /v1/me/playlists": { status: 201, json: { id: "new1", name: "Carro" } },
      "POST /v1/playlists/new1/tracks": { status: 201, json: { snapshot_id: "x" } },
    });
    const s = createSpotify(cfg, f.fetchFn);
    expect(await s.save("Carro")).toEqual({ name: "Oceano", playlist: "Carro" });
    expect(JSON.parse(f.calls.find((c) => c.url.pathname === "/v1/me/playlists" && c.method === "POST")!.body!).name).toBe("Carro");
    expect(JSON.parse(f.calls.find((c) => c.url.pathname === "/v1/playlists/new1/tracks")!.body!)).toEqual({ uris: ["spotify:track:9"] });
  });

  it("reuses an existing playlist, matching the name case-insensitively", async () => {
    const f = fake({
      "POST /api/token": tokenRoute,
      "GET /v1/me/player": { json: playing },
      "GET /v1/me/playlists": { json: { items: [{ id: "p2", name: "CARRO" }], next: null } },
      "POST /v1/playlists/p2/tracks": { status: 201, json: {} },
    });
    const s = createSpotify(cfg, f.fetchFn);
    expect(await s.save("carro")).toEqual({ name: "Oceano", playlist: "CARRO" });
    expect(f.count("POST /v1/me/playlists")).toBe(0);
  });

  it("fails clearly when nothing is playing", async () => {
    const f = fake({ "POST /api/token": tokenRoute, "GET /v1/me/player": { status: 204 } });
    const s = createSpotify(cfg, f.fetchFn);
    await expect(s.save("Carro")).rejects.toBeInstanceOf(SpotifyError);
  });
});

describe("restore", () => {
  const base: Playback = {
    trackUri: "spotify:track:9", contextUri: null, positionMs: 61000,
    isPlaying: true, volume: 30, title: "t", artist: "a",
  };

  it("resumes a playlist/album context at the track and position", async () => {
    const f = fake({ "POST /api/token": tokenRoute, "PUT /v1/me/player/play": { status: 204 } });
    const s = createSpotify(cfg, f.fetchFn);
    await s.restore({ ...base, contextUri: "spotify:playlist:5" });
    expect(JSON.parse(f.calls[1].body!)).toEqual({
      context_uri: "spotify:playlist:5", offset: { uri: "spotify:track:9" }, position_ms: 61000,
    });
  });

  it("falls back to the bare track with no context, and skips the offset for artist contexts", async () => {
    const f = fake({ "POST /api/token": tokenRoute, "PUT /v1/me/player/play": { status: 204 } });
    const s = createSpotify(cfg, f.fetchFn);
    await s.restore(base);
    await s.restore({ ...base, contextUri: "spotify:artist:1" });
    expect(JSON.parse(f.calls[1].body!)).toEqual({ uris: ["spotify:track:9"], position_ms: 61000 });
    expect(JSON.parse(f.calls[2].body!)).toEqual({ context_uri: "spotify:artist:1", position_ms: 61000 });
  });

  it("pauses again if the snapshot was paused", async () => {
    const f = fake({
      "POST /api/token": tokenRoute,
      "PUT /v1/me/player/play": { status: 204 },
      "PUT /v1/me/player/pause": { status: 204 },
    });
    const s = createSpotify(cfg, f.fetchFn);
    await s.restore({ ...base, isPlaying: false });
    expect(f.calls.at(-1)!.url.pathname).toBe("/v1/me/player/pause");
  });
});

describe("timeouts", () => {
  it("gives up on a request that never answers and maps it to a SpotifyError", async () => {
    const signals: (AbortSignal | undefined)[] = [];
    // Never resolves on its own; rejects only when its signal aborts.
    const hang = ((_input: unknown, init?: RequestInit) => new Promise<Response>((_res, rej) => {
      signals.push(init?.signal ?? undefined);
      init?.signal?.addEventListener("abort", () => rej(init.signal!.reason));
    })) as unknown as typeof fetch;
    const s = createSpotify(cfg, hang, 30);
    const err = await s.control("pause").catch((e: unknown) => e);
    expect(err).toBeInstanceOf(SpotifyError);
    expect((err as SpotifyError).code).toBe("other");
    expect((err as SpotifyError).message).toMatch(/timed out/);
    expect(signals).toHaveLength(1);
    expect(signals[0]).toBeInstanceOf(AbortSignal);
  });

  it("puts a timeout signal on every request, the token refresh included", async () => {
    const seen: (AbortSignal | undefined | null)[] = [];
    const f = fake({ "POST /api/token": tokenRoute, "PUT /v1/me/player/pause": { status: 204 } });
    const spy = ((input: unknown, init?: RequestInit) => { seen.push(init?.signal); return f.fetchFn(input as string, init); }) as typeof fetch;
    await createSpotify(cfg, spy).control("pause");
    expect(seen).toHaveLength(2);
    for (const sig of seen) expect(sig).toBeInstanceOf(AbortSignal);
  });
});
