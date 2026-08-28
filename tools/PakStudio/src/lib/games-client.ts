export type ConfigGame = {
  id: string;
  paksDir?: string;
  pakCount?: number;
  dataPak?: string;
  error?: string;
};

export type GamesListResponse = {
  ok: boolean;
  configFound?: boolean;
  configDir?: string;
  games?: ConfigGame[];
  cwd?: string;
  hint?: string;
  error?: string;
};

export type GamesProbeResponse = {
  ok: boolean;
  inputPath?: string;
  paksDir?: string;
  pakCount?: number;
  dataPak?: string;
  matchedGameId?: string;
  source?: string;
  ready?: boolean;
  error?: string;
  cwd?: string;
};

export async function fetchGames(): Promise<GamesListResponse> {
  const res = await fetch("/api/games");
  return res.json() as Promise<GamesListResponse>;
}

export async function probeGamePath(path: string): Promise<GamesProbeResponse> {
  const res = await fetch("/api/games", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ path }),
  });
  return res.json() as Promise<GamesProbeResponse>;
}
