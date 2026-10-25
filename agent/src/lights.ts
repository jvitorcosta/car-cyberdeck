import type { LightAction } from "./protocol.js";

export interface SayQuery { zone?: "inside" | "outside"; text: string; }

// Words understood by the deck's parser (src/util/ledvoice.h).
export function toSayQuery(a: LightAction): SayQuery | null {
  if (a.kind === "undo") return null;
  const zone = a.zone === "both" ? {} : { zone: a.zone };
  if (a.kind === "step") return { ...zone, text: a.direction === "up" ? "brighter" : "dimmer" };
  const words: string[] = [];
  if (a.colour) words.push(a.colour);
  if (a.level !== undefined) {
    const n = Math.min(100, Math.max(0, Math.round(a.level / 10) * 10));
    words.push(String(n));
  }
  if (a.power) words.push(a.power);
  if (words.length === 0) return null;
  return { ...zone, text: words.join(" ") };
}
