import type { PakEntry } from "./types";

export type FolderNode = {
  name: string;
  path: string;
  count: number;
  children: FolderNode[];
};

type FolderBuilder = {
  name: string;
  path: string;
  count: number;
  children: Map<string, FolderBuilder>;
};

function folderBuilderToNode(node: FolderBuilder): FolderNode {
  const children = Array.from(node.children.values())
    .map(folderBuilderToNode)
    .sort((a, b) => a.name.localeCompare(b.name));
  return { name: node.name, path: node.path, count: node.count, children };
}

export function buildFolderTree(entries: PakEntry[]): FolderNode[] {
  const root = new Map<string, FolderBuilder>();

  for (const entry of entries) {
    const parts = entry.virtualPath.split("/").filter(Boolean);
    if (parts.length === 0) continue;

    let level = root;
    let path = "";

    for (let i = 0; i < parts.length - 1; i++) {
      const part = parts[i];
      path = path ? `${path}/${part}` : part;
      if (!level.has(part)) {
        level.set(part, { name: part, path, count: 0, children: new Map() });
      }
      const node = level.get(part)!;
      node.count += 1;
      level = node.children;
    }
  }

  return Array.from(root.values())
    .map(folderBuilderToNode)
    .sort((a, b) => a.name.localeCompare(b.name));
}

export function buildTreeModel(entries: PakEntry[]): Map<string, PakEntry[]> {
  const byPak = new Map<string, PakEntry[]>();
  for (const entry of entries) {
    const list = byPak.get(entry.sourcePak) ?? [];
    list.push(entry);
    byPak.set(entry.sourcePak, list);
  }
  for (const list of byPak.values()) {
    list.sort((a, b) => a.virtualPath.localeCompare(b.virtualPath));
  }
  return byPak;
}

export function filterEntriesByFolder(entries: PakEntry[], folderPath: string): PakEntry[] {
  if (!folderPath) return entries;
  const prefix = folderPath.endsWith("/") ? folderPath : `${folderPath}/`;
  return entries.filter((e) => e.virtualPath.startsWith(prefix));
}

export function filterEntriesByPak(entries: PakEntry[], pakName: string): PakEntry[] {
  if (!pakName) return entries;
  const needle = pakName.toLowerCase();
  return entries.filter((e) => e.sourcePak.toLowerCase() === needle);
}

export function filterEntriesByExtension(entries: PakEntry[], ext: string): PakEntry[] {
  if (!ext) return entries;
  const needle = ext.toLowerCase().replace(/^\./, "");
  return entries.filter((e) => e.extension.toLowerCase() === needle);
}

function compactForMatch(value: string): string {
  return value.toLowerCase().replace(/[^a-z0-9]/g, "");
}

function subsequenceMatch(haystack: string, needle: string): boolean {
  if (!needle) return true;
  let i = 0;
  for (const ch of haystack) {
    if (ch === needle[i]) i += 1;
    if (i === needle.length) return true;
  }
  return false;
}

export function entryMatchesQuery(entry: PakEntry, query: string): boolean {
  const q = query.trim().toLowerCase();
  if (!q) return true;

  const path = entry.virtualPath.toLowerCase();
  if (path.includes(q)) return true;

  const pak = entry.sourcePak.toLowerCase();
  if (pak.includes(q)) return true;

  // Typo-tolerant pak name match only (e.g. "turist" -> NoTourists_P.pak).
  // Do not fuzzy-match paths; subsequence on map names causes false positives.
  const cq = compactForMatch(q);
  if (cq.length >= 4) {
    const pakBase = compactForMatch(pak.replace(/\.pak$/i, ""));
    if (pakBase.includes(cq) || subsequenceMatch(pakBase, cq)) return true;
  }

  return false;
}

export function filterEntriesByQuery(entries: PakEntry[], query: string): PakEntry[] {
  const q = query.trim();
  if (!q) return entries;
  return entries.filter((e) => entryMatchesQuery(e, q));
}

export function collectExtensions(entries: PakEntry[]): { ext: string; count: number }[] {
  const counts = new Map<string, number>();
  for (const entry of entries) {
    const ext = (entry.extension || "?").toLowerCase();
    counts.set(ext, (counts.get(ext) ?? 0) + 1);
  }
  return [...counts.entries()]
    .map(([ext, count]) => ({ ext, count }))
    .sort((a, b) => b.count - a.count);
}

export function collectPakGroups(entries: PakEntry[]): { name: string; count: number }[] {
  const counts = new Map<string, number>();
  for (const entry of entries) {
    counts.set(entry.sourcePak, (counts.get(entry.sourcePak) ?? 0) + 1);
  }
  return [...counts.entries()]
    .map(([name, count]) => ({ name, count }))
    .sort((a, b) => a.name.localeCompare(b.name));
}

export function buildPathSuggestions(entries: PakEntry[], query: string, limit = 10): string[] {
  const q = query.trim();
  if (!q) return [];

  const matched = filterEntriesByQuery(entries, q);
  const paths = new Set<string>();

  for (const entry of matched) {
    paths.add(entry.sourcePak.replace(/\.pak$/i, ""));
    paths.add(entry.virtualPath);
    const parts = entry.virtualPath.split("/").filter(Boolean);
    for (let i = 1; i <= parts.length; i++) {
      paths.add(parts.slice(0, i).join("/"));
    }
  }

  return [...paths]
    .sort((a, b) => {
      const ql = q.toLowerCase();
      const aStarts = a.toLowerCase().startsWith(ql) ? 0 : 1;
      const bStarts = b.toLowerCase().startsWith(ql) ? 0 : 1;
      if (aStarts !== bStarts) return aStarts - bStarts;
      return a.localeCompare(b);
    })
    .slice(0, limit);
}

export function searchMatchSummary(entries: PakEntry[], query: string): string {
  const q = query.trim().toLowerCase();
  if (!q || entries.length === 0) return "";

  let pathHits = 0;
  let pakOnlyHits = 0;
  for (const entry of entries) {
    if (entry.virtualPath.toLowerCase().includes(q)) pathHits += 1;
    else if (entry.sourcePak.toLowerCase().includes(q)) pakOnlyHits += 1;
  }

  if (pathHits === 0 && pakOnlyHits > 0) {
    return "matched pak name only, no file path contains this text";
  }
  if (pathHits > 0 && pakOnlyHits > 0) {
    return `${pathHits} path + ${pakOnlyHits} pak name`;
  }
  return "";
}

export function displayName(entry: PakEntry, folderPath: string): string {
  if (!folderPath) return entry.virtualPath;
  const prefix = `${folderPath}/`;
  if (entry.virtualPath.startsWith(prefix)) {
    return entry.virtualPath.slice(prefix.length);
  }
  return entry.virtualPath.split("/").pop() ?? entry.virtualPath;
}
