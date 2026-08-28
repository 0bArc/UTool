"use client";

import { IconCode, IconHammer, IconHome } from "@/components/icons";
import type { StudioView } from "@/components/SiteNav";

type Props = {
  view: StudioView;
  onNavigate: (view: StudioView) => void;
  canEditor: boolean;
  canBuild: boolean;
};

export function AppRail({ view, onNavigate, canEditor, canBuild }: Props) {
  return (
    <nav className="ide-rail" aria-label="Studio">
      <button
        type="button"
        className={`ide-rail-btn ${view === "welcome" ? "ide-rail-btn-active" : ""}`}
        title="Start"
        onClick={() => onNavigate("welcome")}
      >
        <IconHome size={18} />
      </button>
      <button
        type="button"
        className={`ide-rail-btn ${view === "editor" ? "ide-rail-btn-active" : ""}`}
        title="Edit mod"
        disabled={!canEditor}
        onClick={() => onNavigate("editor")}
      >
        <IconCode size={18} />
      </button>
      <button
        type="button"
        className={`ide-rail-btn ${view === "build" ? "ide-rail-btn-active" : ""}`}
        title="Build"
        disabled={!canBuild}
        onClick={() => onNavigate("build")}
      >
        <IconHammer size={18} />
      </button>
    </nav>
  );
}
