import { existsSync, readdirSync, readFileSync } from "node:fs";
import { dirname, isAbsolute, join, normalize } from "node:path";
import { fileURLToPath } from "node:url";

export type ConfigGame = {
  id: string;
  paksDir?: string;
  pakCount?: number;
  dataPak?: string;
  error?: string;
};

type UtoolJson = {
  games?: Record<string, { paksDir?: string; dataPak?: string }>;
};

export function resolvePakStudioRoot(): string {
  if (process.env.PAK_STUDIO_ROOT) return normalize(process.env.PAK_STUDIO_ROOT);
  const cwd = normalize(process.cwd());
  if (existsSync(join(cwd, "next.config.ts"))) return cwd;
  return normalize(join(dirname(fileURLToPath(import.meta.url)), "..", ".."));
}

export function resolveRepoRoot(): string {
  if (process.env.UTOOL_REPO_ROOT) return normalize(process.env.UTOOL_REPO_ROOT);
  const studio = resolvePakStudioRoot();
  const marker = join(studio, ".repo-root");
  if (existsSync(marker)) {
    try {
      return normalize(readFileSync(marker, "utf8").trim());
    } catch {
      /* fall through */
    }
  }
  return normalize(join(studio, "..", ".."));
}

function scorePaksDir(dir: string): number {
  const lower = dir.toLowerCase();
  let score = 0;
  if (lower.includes("\\engine\\") || lower.includes("/engine/")) score -= 1000;
  if (lower.includes("crashreport")) score -= 1000;

  try {
    const files = readdirSync(dir).filter((f) => f.toLowerCase().endsWith(".pak"));
    score += files.length * 10;
    for (const f of files) {
      const name = f.toLowerCase();
      if (name.includes("pakchunk")) score += 100;
      if (name.includes("crashreport")) score -= 500;
    }
  } catch {
    return -9999;
  }

  return score;
}

function countPaks(dir: string): number {
  let total = 0;
  try {
    for (const entry of readdirSync(dir, { withFileTypes: true })) {
      if (entry.isFile() && entry.name.toLowerCase().endsWith(".pak")) total += 1;
      if (entry.isDirectory()) {
        try {
          for (const nested of readdirSync(join(dir, entry.name), { withFileTypes: true })) {
            if (nested.isFile() && nested.name.toLowerCase().endsWith(".pak")) total += 1;
          }
        } catch {
          /* ignore */
        }
      }
    }
  } catch {
    return 0;
  }
  return total;
}

function findPaksDir(root: string): string | null {
  const candidates: string[] = [];

  const consider = (candidate: string) => {
    if (countPaks(candidate) > 0) candidates.push(normalize(candidate));
  };

  if (!existsSync(root)) return null;

  consider(root);
  consider(join(root, "Content", "Paks"));

  try {
    for (const entry of readdirSync(root, { withFileTypes: true })) {
      if (!entry.isDirectory()) continue;
      consider(join(root, entry.name, "Content", "Paks"));
    }
  } catch {
    /* ignore */
  }

  if (candidates.length === 0) return null;

  candidates.sort((a, b) => scorePaksDir(b) - scorePaksDir(a));
  return candidates[0] ?? null;
}

function configCandidates(): string[] {
  const dirs: string[] = [];
  if (process.env.UTOOL_CONFIG_DIR) dirs.push(process.env.UTOOL_CONFIG_DIR);

  const repo = resolveRepoRoot();
  dirs.push(repo, join(repo, "examples"), resolvePakStudioRoot(), process.cwd());

  const seen = new Set<string>();
  const out: string[] = [];

  for (const start of dirs) {
    let dir = normalize(start);
    for (let i = 0; i < 12; i++) {
      for (const key of [dir, join(dir, "examples")]) {
        if (seen.has(key)) continue;
        seen.add(key);
        if (existsSync(join(key, "utool.json"))) out.push(key);
      }
      const parent = dirname(dir);
      if (parent === dir) break;
      dir = parent;
    }
  }

  return out;
}

export function findConfigDir(): string | null {
  return configCandidates()[0] ?? null;
}

function resolveConfigPath(configDir: string, p: string): string {
  if (isAbsolute(p)) return normalize(p);
  return normalize(join(configDir, p));
}

function readUtoolJson(configDir: string): UtoolJson | null {
  try {
    return JSON.parse(readFileSync(join(configDir, "utool.json"), "utf8")) as UtoolJson;
  } catch {
    return null;
  }
}

export function listGamesFromConfig(): {
  configFound: boolean;
  configDir?: string;
  games: ConfigGame[];
} {
  const configDir = findConfigDir();
  if (!configDir) return { configFound: false, games: [] };

  const doc = readUtoolJson(configDir);
  if (!doc?.games) return { configFound: true, configDir, games: [] };

  const games: ConfigGame[] = [];
  for (const [id, settings] of Object.entries(doc.games)) {
    const g: ConfigGame = { id };
    if (settings.paksDir) {
      try {
        const dir = resolveConfigPath(configDir, settings.paksDir);
        g.paksDir = dir;
        g.pakCount = countPaks(dir);
      } catch (err) {
        g.paksDir = settings.paksDir;
        g.error = err instanceof Error ? err.message : String(err);
      }
    }
    if (settings.dataPak) g.dataPak = settings.dataPak;
    games.push(g);
  }

  return { configFound: true, configDir, games };
}

export function probeInstallPath(inputPath: string): {
  inputPath: string;
  paksDir?: string;
  pakCount: number;
  source: string;
  matchedGameId?: string;
  ready: boolean;
} {
  const normalized = normalize(inputPath.trim());
  const paksDir = findPaksDir(normalized);
  const pakCount = paksDir ? countPaks(paksDir) : 0;
  const ready = pakCount > 0;

  let matchedGameId: string | undefined;
  const { games } = listGamesFromConfig();
  if (paksDir) {
    for (const g of games) {
      if (g.paksDir && normalize(g.paksDir) === normalize(paksDir)) {
        matchedGameId = g.id;
        break;
      }
    }
  }

  return {
    inputPath: normalized,
    paksDir: paksDir ?? undefined,
    pakCount,
    source: matchedGameId ?? paksDir ?? normalized,
    matchedGameId,
    ready,
  };
}

export function utoolCwd(): string {
  return findConfigDir() ?? resolveRepoRoot();
}
