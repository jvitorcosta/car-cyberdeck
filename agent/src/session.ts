export interface Exchange { at: number; user: string; did: string; }   // did: "spotify_play Djavan"
export type UndoFn = () => Promise<void>;

export const CONTEXT_MS = 120_000, UNDO_MS = 10_000;

interface Deck {
  exchanges: Exchange[];
  undo: { fn: UndoFn; at: number } | null;
  tail: Promise<unknown>;
}

export class Sessions {
  private decks = new Map<string, Deck>();

  constructor(private now: () => number = Date.now) {}

  private deck(id: string): Deck {
    let d = this.decks.get(id);
    if (!d) { d = { exchanges: [], undo: null, tail: Promise.resolve() }; this.decks.set(id, d); }
    return d;
  }

  context(deck: string): Exchange[] {
    const d = this.deck(deck);
    const cutoff = this.now() - CONTEXT_MS;
    d.exchanges = d.exchanges.filter((e) => e.at > cutoff);
    return [...d.exchanges];
  }

  remember(deck: string, e: Omit<Exchange, "at">): void {
    this.context(deck); // drop the expired ones
    this.deck(deck).exchanges.push({ ...e, at: this.now() });
  }

  setUndo(deck: string, fn: UndoFn): void {
    this.deck(deck).undo = { fn, at: this.now() };
  }

  takeUndo(deck: string): UndoFn | null {
    const d = this.deck(deck);
    const u = d.undo;
    d.undo = null;
    return u && this.now() - u.at < UNDO_MS ? u.fn : null;
  }

  run<T>(deck: string, job: () => Promise<T>): Promise<T> {
    const d = this.deck(deck);
    const result = d.tail.then(job);
    d.tail = result.catch(() => undefined); // a failed job must not block the next one
    return result;
  }
}
