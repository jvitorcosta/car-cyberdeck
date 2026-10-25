# Voice agent evals

Text evals for Zero Two's fast path: each case in `agent/evals/cases.jsonl` goes through
`handleCommand` with the real model, a recording Spotify fake and a slow-path stub that only records
the handoff. Every case runs in fresh sessions; a case with a `before` list runs those texts first in
the same session (follow-ups and undo), and only its last text is judged.

Verdicts: **right**; **wrong action** (a tool ran that is not the expected one, or with the wrong
arguments; the worst); **asked** (an ask, a hand-off or no tool when an action was expected: a
deferral, nothing changes on the fast path); **missed** (no tool when an ask or a handoff was expected).
A case's optional `alsoOk` (`"handoff"` / `"ask"`) makes that outcome right too, for commands that are
genuinely two-way. If a `before` text produced no tool, the case is reported as setup-failed and left
out of the scores (the row's n says how many). Latency is the wall time of the judged command; cost is
the fast-path model call only.

Run (costs real credits, a fraction of a cent on Haiku), from `agent/`:

```
npm run eval -- --model claude-haiku-5-5
npm run eval -- --model claude-sonnet-5-5
```

Each run appends one row below, so the table is the history. The first rows come from the owner's
first run of the two commands above (no API key on the build machine).

| date | model | n | accuracy | wrong action | asked | p50 / p95 ms | cost / cmd |
|---|---|---|---|---|---|---|---|
