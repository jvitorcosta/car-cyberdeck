export type Zone = "inside" | "outside" | "both";
export type Colour = "red" | "orange" | "pink" | "purple" | "blue" | "green" | "white" | "yellow";
export type LightAction =
  | { kind: "set"; zone: Zone; colour?: Colour; level?: number; power?: "on" | "off" }
  | { kind: "step"; zone: Zone; direction: "up" | "down" }
  | { kind: "undo" }; // the deck restores what it read before
export type StockId =
  | "ok" | "tocando" | "pulando" | "pausado" | "desfeito" | "nao_entendi"
  | "deixa_comigo" | "sem_spotify" | "nada_desfazer" | "erro";
export interface CommandRequest { deck: string; text: string; }
export interface Reply {
  heard: string; lights: LightAction[]; stock: StockId; say: string;
  mood: "happy" | "smug" | "talk"; path: "fast" | "slow" | "none"; traceId: string;
}
