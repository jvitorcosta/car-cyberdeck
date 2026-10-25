import { describe, expect, it } from "vitest";
import { toSayQuery } from "../src/lights.js";

describe("toSayQuery", () => {
  it("maps set and step actions to the deck's words", () => {
    expect(toSayQuery({ kind: "set", zone: "inside", colour: "blue", level: 33 }))
      .toEqual({ zone: "inside", text: "blue 30" });
    expect(toSayQuery({ kind: "set", zone: "outside", power: "off" })).toEqual({ zone: "outside", text: "off" });
    expect(toSayQuery({ kind: "set", zone: "both", colour: "purple" })).toEqual({ text: "purple" });
    expect(toSayQuery({ kind: "step", zone: "both", direction: "down" })).toEqual({ text: "dimmer" });
    expect(toSayQuery({ kind: "step", zone: "outside", direction: "up" })).toEqual({ zone: "outside", text: "brighter" });
    expect(toSayQuery({ kind: "set", zone: "both", level: 100 })).toEqual({ text: "100" });
    expect(toSayQuery({ kind: "undo" })).toBeNull();
  });
  it("clamps levels to 0-100 and returns null for an empty set", () => {
    expect(toSayQuery({ kind: "set", zone: "both", level: 250 })).toEqual({ text: "100" });
    expect(toSayQuery({ kind: "set", zone: "both", level: -5 })).toEqual({ text: "0" });
    expect(toSayQuery({ kind: "set", zone: "both" })).toBeNull();
  });
});
