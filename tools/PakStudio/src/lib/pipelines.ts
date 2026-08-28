import { existsSync, cpSync, mkdirSync, readdirSync, readFileSync, rmSync, statSync, writeFileSync } from "node:fs";
import { basename, dirname, isAbsolute, join, normalize, relative, resolve } from "node:path";
import { resolvePakStudioRoot, resolveRepoRoot } from "@/lib/utool-config";

export type PipelineInfo = {
  id: string;
  path: string;
  name: string;
  files: string[];
  source: "manifest" | "discover" | "user";
};

type ManifestDoc = {
  pipelines?: Array<{ id?: string; path?: string }>;
};

function pipelinesDir(): string {
  return join(resolvePakStudioRoot(), "pipelines");
}

function resolvePipelinePath(relOrAbs: string): string {
  if (isAbsolute(relOrAbs)) return normalize(relOrAbs);
  return normalize(resolve(pipelinesDir(), relOrAbs));
}

const SKIP_DIRS = new Set(["node_modules", ".git"]);

function isListedFile(name: string, underDist: boolean): boolean {
  const lower = name.toLowerCase();
  if (
    lower.endsWith(".lua") ||
    lower.endsWith(".json") ||
    lower.endsWith(".md") ||
    lower.endsWith(".txt")
  ) {
    return true;
  }
  if (underDist && (lower.endsWith(".pak") || lower.endsWith(".zip"))) return true;
  return false;
}

function listProjectTree(root: string, base = "", underDist = false): string[] {
  const out: string[] = [];
  let entries;
  try {
    entries = readdirSync(join(root, base), { withFileTypes: true });
  } catch {
    return out;
  }
  for (const entry of entries) {
    if (entry.name.startsWith(".") || SKIP_DIRS.has(entry.name)) continue;
    const rel = base ? `${base}/${entry.name}` : entry.name;
    const nextUnderDist = underDist || entry.name === "dist";
    if (entry.isDirectory()) {
      const nested = listProjectTree(root, rel, nextUnderDist);
      out.push(...nested);
      if (nested.length === 0) out.push(`${rel.replace(/\\/g, "/")}/`);
      continue;
    }
    if (isListedFile(entry.name, underDist)) out.push(rel.replace(/\\/g, "/"));
  }
  return out.sort((a, b) => a.localeCompare(b));
}

function readModName(modDir: string, fallback: string): string {
  const modLua = join(modDir, "mod.lua");
  if (!existsSync(modLua)) return fallback;
  try {
    const text = readFileSync(modLua, "utf8");
    const m = text.match(/name\s*=\s*"([^"]+)"/);
    return m?.[1] ?? fallback;
  } catch {
    return fallback;
  }
}

function slugId(raw: string): string {
  const cleaned = raw
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "")
    .slice(0, 48);
  return cleaned || "new-mod";
}

function userManifestPath(): string {
  return join(pipelinesDir(), "user.json");
}

function readUserManifest(): ManifestDoc {
  const path = userManifestPath();
  if (!existsSync(path)) return { pipelines: [] };
  try {
    return JSON.parse(readFileSync(path, "utf8")) as ManifestDoc;
  } catch {
    return { pipelines: [] };
  }
}

function writeUserManifest(doc: ManifestDoc): void {
  mkdirSync(pipelinesDir(), { recursive: true });
  writeFileSync(userManifestPath(), `${JSON.stringify(doc, null, 2)}\n`, "utf8");
}

function uniquePipelineId(base: string, taken: Set<string>): string {
  let id = slugId(base);
  let n = 2;
  while (taken.has(id)) {
    id = `${slugId(base)}-${n}`;
    n += 1;
  }
  return id;
}

function pipelineFromDir(id: string, abs: string, source: PipelineInfo["source"]): PipelineInfo {
  return {
    id,
    path: abs,
    name: readModName(abs, id),
    files: listProjectTree(abs),
    source,
  };
}

export function listPipelines(): PipelineInfo[] {
  const byId = new Map<string, PipelineInfo>();

  const manifestPath = join(pipelinesDir(), "manifest.json");
  if (existsSync(manifestPath)) {
    try {
      const doc = JSON.parse(readFileSync(manifestPath, "utf8")) as ManifestDoc;
      for (const item of doc.pipelines ?? []) {
        if (!item.id || !item.path) continue;
        const abs = resolvePipelinePath(item.path);
        if (!existsSync(join(abs, "mod.lua"))) continue;
        byId.set(item.id, pipelineFromDir(item.id, abs, "manifest"));
      }
    } catch {
      /* ignore bad manifest */
    }
  }

  const examples = join(resolveRepoRoot(), "examples");
  if (existsSync(examples)) {
    try {
      for (const entry of readdirSync(examples, { withFileTypes: true })) {
        if (!entry.isDirectory()) continue;
        const abs = join(examples, entry.name);
        if (!existsSync(join(abs, "mod.lua"))) continue;
        if (byId.has(entry.name)) continue;
        byId.set(entry.name, pipelineFromDir(entry.name, abs, "discover"));
      }
    } catch {
      /* ignore */
    }
  }

  for (const item of readUserManifest().pipelines ?? []) {
    if (!item.id || !item.path) continue;
    const abs = resolvePipelinePath(item.path);
    if (!existsSync(join(abs, "mod.lua"))) continue;
    byId.set(item.id, pipelineFromDir(item.id, abs, "user"));
  }

  return [...byId.values()].sort((a, b) => a.id.localeCompare(b.id));
}

export function getPipeline(id: string): PipelineInfo | null {
  return listPipelines().find((p) => p.id === id) ?? null;
}

export function createPipelineFromExample(requestedId?: string, displayName?: string): PipelineInfo {
  const examples = join(resolveRepoRoot(), "examples");
  const template = join(examples, "example-mod");
  if (!existsSync(join(template, "mod.lua"))) {
    throw new Error("examples/example-mod/mod.lua not found");
  }

  const taken = new Set(listPipelines().map((p) => p.id));
  let id = uniquePipelineId(requestedId ?? displayName ?? `mod-${Date.now().toString(36)}`, taken);
  let dest = join(examples, id);
  let n = 2;
  while (existsSync(dest)) {
    id = uniquePipelineId(`${requestedId ?? displayName ?? "mod"}-${n}`, taken);
    dest = join(examples, id);
    n += 1;
    taken.add(id);
  }

  mkdirSync(dest, { recursive: true });
  cpSync(template, dest, { recursive: true });
  stampModLua(dest, id, displayName ?? id);
  rememberUserPipeline(id, dest);
  return pipelineFromDir(id, dest, "user");
}

function stampModLua(dest: string, id: string, displayName: string): void {
  const modLua = join(dest, "mod.lua");
  let text = readFileSync(modLua, "utf8");
  text = text.replace(/id\s*=\s*"[^"]*"/, `id = "local.${id.replace(/-/g, ".")}"`);
  text = text.replace(/name\s*=\s*"[^"]*"/, `name = "${displayName.replace(/"/g, '\\"')}"`);
  if (!/mountPoint\s*=\s*"@auto"/.test(text)) {
    text = text.replace(/mountPoint\s*=\s*"[^"]*"/, `mountPoint = "@auto"`);
  }
  writeFileSync(modLua, text, "utf8");
}

function rememberUserPipeline(id: string, abs: string): void {
  const doc = readUserManifest();
  const pipelines = doc.pipelines ?? [];
  const existing = pipelines.findIndex((p) => p.id === id || (p.path && normalize(p.path) === abs));
  const entry = { id, path: abs };
  if (existing >= 0) pipelines[existing] = entry;
  else pipelines.push(entry);
  writeUserManifest({ pipelines });
}

export function openOrCreatePipelineAt(folderPath: string, options?: { create?: boolean; name?: string }): PipelineInfo {
  const abs = normalize(isAbsolute(folderPath) ? folderPath : resolve(resolveRepoRoot(), folderPath));
  const template = join(resolveRepoRoot(), "examples", "example-mod");
  const hasMod = existsSync(join(abs, "mod.lua"));

  if (!hasMod) {
    if (!options?.create) {
      throw new Error(`No mod.lua in ${abs}. Use New project, or Open folder with create.`);
    }
    if (!existsSync(join(template, "mod.lua"))) {
      throw new Error("examples/example-mod/mod.lua not found");
    }
    mkdirSync(abs, { recursive: true });
    cpSync(template, abs, { recursive: true });
    const idHint = options.name ?? basename(abs);
    stampModLua(abs, slugId(idHint), options.name ?? basename(abs));
  }

  const existing = listPipelines().find((p) => normalize(p.path) === abs);
  if (existing) return existing;

  const taken = new Set(listPipelines().map((p) => p.id));
  const id = uniquePipelineId(options?.name ?? basename(abs), taken);
  rememberUserPipeline(id, abs);
  return pipelineFromDir(id, abs, "user");
}

export function resolveSafeFile(pipelineId: string, relPath: string): { root: string; abs: string } {
  const pipeline = getPipeline(pipelineId);
  if (!pipeline) throw new Error(`Unknown pipeline: ${pipelineId}`);

  const cleaned = relPath.replace(/\\/g, "/").replace(/^\/+/, "").replace(/\/+$/, "");
  if (!cleaned || cleaned.includes("..")) throw new Error("Invalid path");

  const abs = normalize(join(pipeline.path, cleaned));
  const root = normalize(pipeline.path);
  const rel = relative(root, abs);
  if (rel.startsWith("..") || isAbsolute(rel)) throw new Error("Path escapes pipeline root");

  return { root, abs };
}

export function readPipelineFile(pipelineId: string, relPath: string): string {
  const { abs } = resolveSafeFile(pipelineId, relPath);
  if (!existsSync(abs) || !statSync(abs).isFile()) throw new Error(`File not found: ${relPath}`);
  return readFileSync(abs, "utf8");
}

export function writePipelineFile(pipelineId: string, relPath: string, content: string): void {
  const { abs } = resolveSafeFile(pipelineId, relPath);
  const dir = dirname(abs);
  if (!existsSync(dir)) mkdirSync(dir, { recursive: true });
  writeFileSync(abs, content, "utf8");
}

const DEFAULT_LUA_STUB = `-- New script for this mod\n`;

export function createPipelineEntry(
  pipelineId: string,
  relPath: string,
  kind: "file" | "dir",
  content?: string,
): PipelineInfo {
  const { abs } = resolveSafeFile(pipelineId, relPath);
  if (existsSync(abs)) throw new Error(`Already exists: ${relPath}`);

  if (kind === "dir") {
    mkdirSync(abs, { recursive: true });
  } else {
    mkdirSync(dirname(abs), { recursive: true });
    const body =
      content ??
      (relPath.toLowerCase().endsWith(".lua") ? DEFAULT_LUA_STUB : "");
    writeFileSync(abs, body, "utf8");
  }

  const pipeline = getPipeline(pipelineId);
  if (!pipeline) throw new Error(`Unknown pipeline: ${pipelineId}`);
  return pipelineFromDir(pipeline.id, pipeline.path, pipeline.source);
}

export function deletePipelineEntry(pipelineId: string, relPath: string): PipelineInfo {
  const cleaned = relPath.replace(/\\/g, "/").replace(/^\/+/, "").replace(/\/+$/, "");
  if (!cleaned) throw new Error("Invalid path");
  if (cleaned === "mod.lua") throw new Error("Cannot delete mod.lua");

  const { root, abs } = resolveSafeFile(pipelineId, cleaned);
  if (normalize(abs) === normalize(root)) throw new Error("Cannot delete project root");
  if (!existsSync(abs)) throw new Error(`Not found: ${cleaned}`);

  rmSync(abs, { recursive: true, force: true });

  const pipeline = getPipeline(pipelineId);
  if (!pipeline) throw new Error(`Unknown pipeline: ${pipelineId}`);
  return pipelineFromDir(pipeline.id, pipeline.path, pipeline.source);
}

export function refreshPipeline(pipelineId: string): PipelineInfo {
  const pipeline = getPipeline(pipelineId);
  if (!pipeline) throw new Error(`Unknown pipeline: ${pipelineId}`);
  return pipelineFromDir(pipeline.id, pipeline.path, pipeline.source);
}

function forgetUserPipeline(id: string, abs: string): void {
  const doc = readUserManifest();
  const pipelines = (doc.pipelines ?? []).filter(
    (p) => p.id !== id && !(p.path && normalize(p.path) === normalize(abs)),
  );
  writeUserManifest({ pipelines });
}

/** Remove a project from the studio list and delete its folder on disk. */
export function removePipeline(pipelineId: string): void {
  const pipeline = getPipeline(pipelineId);
  if (!pipeline) throw new Error(`Unknown pipeline: ${pipelineId}`);

  const abs = normalize(pipeline.path);
  const repo = normalize(resolveRepoRoot());
  const examples = normalize(join(repo, "examples"));
  const rel = relative(repo, abs);
  if (!rel || rel.startsWith("..") || isAbsolute(rel)) {
    throw new Error("Refusing to delete project outside the repo");
  }
  if (normalize(abs) === repo || normalize(abs) === examples) {
    throw new Error("Refusing to delete repo root");
  }
  if (!existsSync(join(abs, "mod.lua"))) {
    throw new Error("Not a mod project (missing mod.lua)");
  }

  forgetUserPipeline(pipeline.id, abs);
  rmSync(abs, { recursive: true, force: true });
}
