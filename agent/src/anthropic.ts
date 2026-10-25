import Anthropic from "@anthropic-ai/sdk";

// Every command waits in its deck's queue, so one stalled call must not hold the deck for minutes
// (the SDK default is 600 s with 2 retries). A timeout throws, and the server answers erro.
export const MODEL_TIMEOUT_MS = 5_000;
export const MODEL_MAX_RETRIES = 1;

export function createAnthropic(apiKey: string): Anthropic {
  return new Anthropic({ apiKey, timeout: MODEL_TIMEOUT_MS, maxRetries: MODEL_MAX_RETRIES });
}
