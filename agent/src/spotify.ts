export type Kind = "track" | "artist" | "album" | "playlist";

export interface Playback {
  trackUri: string;
  contextUri: string | null;
  positionMs: number;
  isPlaying: boolean;
  volume: number | null;
  title: string;
  artist: string;
}

export type SpotifyErrorCode = "no_device" | "not_found" | "auth" | "other";

export class SpotifyError extends Error {
  constructor(public code: SpotifyErrorCode, msg: string) {
    super(msg);
    this.name = "SpotifyError";
  }
}

export interface Spotify {
  now(): Promise<Playback | null>;
  play(query: string, kind: Kind): Promise<{ name: string }>;
  control(action: "pause" | "resume" | "next" | "previous"): Promise<void>;
  volume(level: number): Promise<void>;
  queue(query: string): Promise<{ name: string }>;
  save(playlist: string): Promise<{ name: string; playlist: string }>;
  restore(p: Playback): Promise<void>;
}

export interface SpotifyConfig { clientId: string; clientSecret: string; refreshToken: string; }

const API = "https://api.spotify.com/v1";
const TOKEN_URL = "https://accounts.spotify.com/api/token";
const EXPIRY_MARGIN_MS = 30_000;
/** Each request's limit: commands wait in the deck's queue, so a stalled call must not hold it. */
export const SPOTIFY_TIMEOUT_MS = 4_000;

interface ApiTrack { uri: string; name: string; artists?: { name: string }[]; }
interface ApiPlayer {
  is_playing?: boolean;
  progress_ms?: number | null;
  device?: { volume_percent?: number | null } | null;
  context?: { uri?: string } | null;
  item?: ApiTrack | null;
}
interface ApiItem { uri: string; name: string; }

export function createSpotify(cfg: SpotifyConfig, fetchFn: typeof fetch = fetch, timeoutMs = SPOTIFY_TIMEOUT_MS): Spotify {
  let accessToken: string | null = null;
  let expiresAt = 0;

  // fetch with a deadline; a timeout becomes a SpotifyError, which the tools map to the erro reply.
  async function timedFetch(what: string, url: string, init: RequestInit): Promise<Response> {
    try {
      return await fetchFn(url, { ...init, signal: AbortSignal.timeout(timeoutMs) });
    } catch (e) {
      const name = (e as { name?: string } | null)?.name;
      if (name === "TimeoutError" || name === "AbortError") {
        throw new SpotifyError("other", `Spotify ${what} timed out after ${timeoutMs} ms`);
      }
      throw e;
    }
  }

  async function refresh(): Promise<string> {
    const basic = Buffer.from(`${cfg.clientId}:${cfg.clientSecret}`).toString("base64");
    const res = await timedFetch("token refresh", TOKEN_URL, {
      method: "POST",
      headers: { Authorization: `Basic ${basic}`, "Content-Type": "application/x-www-form-urlencoded" },
      body: new URLSearchParams({ grant_type: "refresh_token", refresh_token: cfg.refreshToken }).toString(),
    });
    if (!res.ok) throw new SpotifyError("auth", `token refresh failed (${res.status})`);
    const body = (await res.json()) as { access_token?: string; expires_in?: number };
    if (!body.access_token) throw new SpotifyError("auth", "token refresh returned no access_token");
    accessToken = body.access_token;
    expiresAt = Date.now() + (body.expires_in ?? 3600) * 1000 - EXPIRY_MARGIN_MS;
    return accessToken;
  }

  async function token(): Promise<string> {
    return accessToken && Date.now() < expiresAt ? accessToken : refresh();
  }

  // One request; a 401 forces a single refresh and a single retry.
  async function call(method: string, path: string, opts: { query?: Record<string, string>; body?: unknown } = {}): Promise<Response> {
    const url = new URL(API + path);
    for (const [k, v] of Object.entries(opts.query ?? {})) url.searchParams.set(k, v);
    const send = async (tok: string) => {
      const headers: Record<string, string> = { Authorization: `Bearer ${tok}` };
      if (opts.body !== undefined) headers["Content-Type"] = "application/json";
      return timedFetch(`${method} ${path}`, url.toString(), {
        method, headers, body: opts.body === undefined ? undefined : JSON.stringify(opts.body),
      });
    };
    let res = await send(await token());
    if (res.status === 401) {
      res = await send(await refresh());
      if (res.status === 401) throw new SpotifyError("auth", "Spotify rejected the refreshed token");
    }
    if (res.ok) return res;
    let reason = "";
    let message = "";
    try {
      const e = ((await res.json()) as { error?: { reason?: string; message?: string } }).error;
      reason = e?.reason ?? "";
      message = e?.message ?? "";
    } catch { /* non-JSON error body */ }
    if (reason === "NO_ACTIVE_DEVICE") throw new SpotifyError("no_device", "no active Spotify device");
    throw new SpotifyError("other", `Spotify ${method} ${path} failed (${res.status})${message ? `: ${message}` : ""}`);
  }

  async function search(query: string, kind: Kind): Promise<ApiItem> {
    const res = await call("GET", "/search", { query: { q: query, type: kind, limit: "1", market: "from_token" } });
    const body = (await res.json()) as Record<string, { items?: ApiItem[] } | undefined>;
    const hit = body[`${kind}s`]?.items?.[0];
    if (!hit) throw new SpotifyError("not_found", `nothing found for "${query}" (${kind})`);
    return hit;
  }

  async function now(): Promise<Playback | null> {
    const res = await call("GET", "/me/player");
    if (res.status === 204) return null;
    const p = (await res.json()) as ApiPlayer;
    if (!p.item) return null;
    return {
      trackUri: p.item.uri,
      contextUri: p.context?.uri ?? null,
      positionMs: p.progress_ms ?? 0,
      isPlaying: p.is_playing ?? false,
      volume: p.device?.volume_percent ?? null,
      title: p.item.name,
      artist: (p.item.artists ?? []).map((a) => a.name).join(", "),
    };
  }

  async function findOrCreatePlaylist(name: string): Promise<{ id: string; name: string }> {
    const wanted = name.trim().toLowerCase();
    let next: string | null = "/me/playlists?limit=50";
    while (next) {
      const [path, qs] = next.split("?");
      const query = Object.fromEntries(new URLSearchParams(qs));
      const res = await call("GET", path, { query });
      const page = (await res.json()) as { items?: { id: string; name: string }[]; next?: string | null };
      const hit = (page.items ?? []).find((p) => p.name.trim().toLowerCase() === wanted);
      if (hit) return hit;
      next = page.next ? page.next.replace(API, "") : null;
    }
    const res = await call("POST", "/me/playlists", { body: { name: name.trim(), public: false } });
    const made = (await res.json()) as { id: string; name: string };
    return { id: made.id, name: made.name ?? name.trim() };
  }

  return {
    now,

    async play(query, kind) {
      const hit = await search(query, kind);
      await call("PUT", "/me/player/play", {
        body: kind === "track" ? { uris: [hit.uri] } : { context_uri: hit.uri },
      });
      return { name: hit.name };
    },

    async control(action) {
      if (action === "pause") await call("PUT", "/me/player/pause");
      else if (action === "resume") await call("PUT", "/me/player/play");
      else await call("POST", `/me/player/${action}`);
    },

    async volume(level) {
      const pct = Math.min(100, Math.max(0, Math.round(level)));
      await call("PUT", "/me/player/volume", { query: { volume_percent: String(pct) } });
    },

    async queue(query) {
      const hit = await search(query, "track");
      await call("POST", "/me/player/queue", { query: { uri: hit.uri } });
      return { name: hit.name };
    },

    async save(playlist) {
      const current = await now();
      if (!current) throw new SpotifyError("other", "nothing is playing, so there is nothing to save");
      const target = await findOrCreatePlaylist(playlist);
      await call("POST", `/playlists/${target.id}/tracks`, { body: { uris: [current.trackUri] } });
      return { name: current.title, playlist: target.name };
    },

    async restore(p) {
      const body: Record<string, unknown> = {};
      if (p.contextUri) {
        body.context_uri = p.contextUri;
        // Artist contexts do not support an offset.
        if (!p.contextUri.startsWith("spotify:artist:")) body.offset = { uri: p.trackUri };
      } else {
        body.uris = [p.trackUri];
      }
      body.position_ms = p.positionMs;
      await call("PUT", "/me/player/play", { body });
      if (!p.isPlaying) await call("PUT", "/me/player/pause");
    },
  };
}
