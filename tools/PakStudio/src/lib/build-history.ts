import type { BuildRecord } from "@/components/BuildView";

const KEY = "pak-studio:build-history";
const MAX = 40;

export function loadBuildHistory(): BuildRecord[] {
  if (typeof window === "undefined") return [];
  try {
    const raw = window.localStorage.getItem(KEY);
    if (!raw) return [];
    const parsed = JSON.parse(raw) as BuildRecord[];
    if (!Array.isArray(parsed)) return [];
    return parsed
      .filter(
        (r) =>
          r &&
          typeof r.id === "string" &&
          typeof r.name === "string" &&
          typeof r.path === "string" &&
          typeof r.ok === "boolean" &&
          typeof r.at === "string",
      )
      .slice(0, MAX);
  } catch {
    return [];
  }
}

export function saveBuildHistory(records: BuildRecord[]): void {
  if (typeof window === "undefined") return;
  try {
    window.localStorage.setItem(KEY, JSON.stringify(records.slice(0, MAX)));
  } catch {
    /* quota / private mode */
  }
}
