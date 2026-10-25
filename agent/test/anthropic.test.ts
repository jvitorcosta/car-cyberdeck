import { describe, expect, it } from "vitest";
import { createAnthropic } from "../src/anthropic.js";

describe("createAnthropic", () => {
  it("bounds every model call: 5 s timeout and a single retry", () => {
    const client = createAnthropic("sk-test");
    expect(client.timeout).toBe(5_000);
    expect(client.maxRetries).toBe(1);
  });
});
