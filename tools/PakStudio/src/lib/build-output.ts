export function parseOutputPak(log: string): string {
  const matches = [...log.matchAll(/Built mod pak \(UnrealPak\):\s*(.+)/g)];
  return matches.at(-1)?.[1]?.trim() ?? "";
}

export function parseZippedPath(log: string): string {
  const matches = [...log.matchAll(/Zipped:\s*(.+)/g)];
  return matches.at(-1)?.[1]?.trim() ?? "";
}

/** Prefer the zip when present — the pak is often removed after zipping. */
export function parseBuildArtifact(log: string): string {
  return parseZippedPath(log) || parseOutputPak(log);
}
