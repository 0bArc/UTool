import type { BrowseMode } from "@/components/AssetSidebar";
import type { StartStep } from "@/components/WelcomeView";
import type { StudioView } from "@/components/SiteNav";

const KEY = "utool-studio:session";

export type StudioSession = {
  view: StudioView;
  pipelineId: string;
  openFiles: string[];
  activeFile: string;
  startStep: StartStep;
  gameSource: string;
  gameLabel: string;
  gamePath: string;
  browseMode: BrowseMode;
};

const VIEWS = new Set<StudioView>(["welcome", "editor", "build"]);
const STEPS = new Set<StartStep>(["project", "game"]);

export function loadStudioSession(): StudioSession | null {
  if (typeof window === "undefined") return null;
  try {
    const raw = window.localStorage.getItem(KEY);
    if (!raw) return null;
    const parsed = JSON.parse(raw) as Partial<StudioSession>;
    if (!parsed || typeof parsed !== "object") return null;
    if (!parsed.view || !VIEWS.has(parsed.view)) return null;
    return {
      view: parsed.view,
      pipelineId: typeof parsed.pipelineId === "string" ? parsed.pipelineId : "",
      openFiles: Array.isArray(parsed.openFiles)
        ? parsed.openFiles.filter((f): f is string => typeof f === "string")
        : [],
      activeFile: typeof parsed.activeFile === "string" ? parsed.activeFile : "",
      startStep: parsed.startStep && STEPS.has(parsed.startStep) ? parsed.startStep : "project",
      gameSource: typeof parsed.gameSource === "string" ? parsed.gameSource : "",
      gameLabel: typeof parsed.gameLabel === "string" ? parsed.gameLabel : "",
      gamePath: typeof parsed.gamePath === "string" ? parsed.gamePath : "",
      browseMode: parsed.browseMode === "all" ? "all" : "json",
    };
  } catch {
    return null;
  }
}

export function saveStudioSession(session: StudioSession): void {
  if (typeof window === "undefined") return;
  try {
    window.localStorage.setItem(KEY, JSON.stringify(session));
  } catch {
    /* quota / private mode */
  }
}
