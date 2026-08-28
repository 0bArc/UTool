"use client";

export type StudioView = "welcome" | "editor" | "build";

type Props = {
  view: StudioView;
  onNavigate: (view: StudioView) => void;
  canEditor?: boolean;
  canBuild?: boolean;
};

const STEPS: Array<{ n: 1 | 2 | 3; label: string; view: StudioView }> = [
  { n: 1, label: "Start", view: "welcome" },
  { n: 2, label: "Edit Mod", view: "editor" },
  { n: 3, label: "Build", view: "build" },
];

function locked(view: StudioView, p: Props): boolean {
  if (view === "editor") return !p.canEditor;
  if (view === "build") return !p.canBuild;
  return false;
}

export function SiteNav({
  view,
  onNavigate,
  canEditor = false,
  canBuild = false,
}: Props) {
  return (
    <nav className="ide-top">
      <a href="/" className="ide-brand">
        <span className="ide-logo-slot" aria-hidden>
          <span className="ide-logo">U</span>
        </span>
        <span className="ide-brand-name">UTool Studio</span>
      </a>
      <ol className="ide-steps">
        {STEPS.map((s) => (
          <li key={s.n} className={`ide-step ${view === s.view ? "ide-step-active" : ""}`}>
            <button
              type="button"
              className="ide-step-btn"
              disabled={locked(s.view, { view, onNavigate, canEditor, canBuild })}
              onClick={() => onNavigate(s.view)}
            >
              <span className="ide-step-n">{s.n}</span>
              <span className="ide-step-label">{s.label}</span>
            </button>
          </li>
        ))}
      </ol>
      <div className="ide-top-right">
        <a href="https://stratware.win" className="ide-top-link" target="_blank" rel="noreferrer">
          Stratware
        </a>
      </div>
    </nav>
  );
}
