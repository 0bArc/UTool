"use client";

import { useContextMenu } from "@/components/ContextMenuProvider";
import type { ConfigGame } from "@/lib/games-client";
import type { PipelineInfo } from "@/lib/pipelines-client";

export type StartStep = "project" | "game";

type Props = {
  step: StartStep;
  games: ConfigGame[];
  pipelines: PipelineInfo[];
  pathInput: string;
  onPathInput: (v: string) => void;
  busy: boolean;
  gameLoaded: boolean;
  activeGameId: string;
  projectName: string;
  onProjectName: (v: string) => void;
  projectFolder: string;
  onProjectFolder: (v: string) => void;
  onPickGame: (g: ConfigGame) => void;
  onScanPath: () => void;
  onOpenPipeline: (id: string) => void;
  onNewProject: () => void;
  onOpenFolder: () => void;
  onOpenEditor: () => void;
  onBackToProjects: () => void;
  onDeletePipeline: (id: string, name: string) => void;
  onRevealPipeline: (id: string) => void;
  projectLabel?: string;
};

export function WelcomeView({
  step,
  games,
  pipelines,
  pathInput,
  onPathInput,
  busy,
  gameLoaded,
  activeGameId,
  projectName,
  onProjectName,
  projectFolder,
  onProjectFolder,
  onPickGame,
  onScanPath,
  onOpenPipeline,
  onNewProject,
  onOpenFolder,
  onOpenEditor,
  onBackToProjects,
  onDeletePipeline,
  onRevealPipeline,
  projectLabel,
}: Props) {
  const { open } = useContextMenu();
  const onGameStep = step === "game";

  return (
    <div
      className="ws-main start-hub"
      onContextMenu={(e) => {
        open(
          e,
          onGameStep
            ? [
                { id: "back", label: "Back to projects" },
                { id: "sep", label: "", separator: true },
                { id: "edit", label: "Open editor", disabled: !gameLoaded },
              ]
            : [
                { id: "new-project", label: "New Project…" },
                { id: "open-folder", label: "Open Folder…" },
              ],
          (id) => {
            if (id === "new-project") onNewProject();
            else if (id === "open-folder") onOpenFolder();
            else if (id === "back") onBackToProjects();
            else if (id === "edit") onOpenEditor();
          },
        );
      }}
    >
      {onGameStep ? (
        <>
          <header className="ws-header">
            <div>
              <p className="start-step-eyebrow">Step 2 of 2</p>
              <h1 className="ws-title">Load game</h1>
              <p className="ws-sub">
                {projectLabel
                  ? `Choose a game for ${projectLabel}.`
                  : "Choose a game to browse JSON tables."}
              </p>
            </div>
          </header>

          <section className="start-game-panel">
            <p className="sb-label">Configured games</p>
            <p className="ide-muted">
              Auto-selected when mod.lua has a gameId. You can change it anytime.
            </p>
            <div className="game-list" style={{ marginTop: 12 }}>
              {games.length === 0 ? (
                <p className="ide-muted">No games in utool.json.</p>
              ) : (
                games.map((g) => (
                  <button
                    key={g.id}
                    type="button"
                    className={`game-row ${activeGameId === g.id ? "game-row-active" : ""}`}
                    disabled={busy}
                    onClick={() => onPickGame(g)}
                    title={g.paksDir}
                  >
                    <span>
                      <span className="game-row-name">
                        {g.id}
                        {g.pakCount != null ? ` (${g.pakCount})` : ""}
                      </span>
                      {g.paksDir ? <span className="game-row-path">{g.paksDir}</span> : null}
                    </span>
                    {activeGameId === g.id && gameLoaded ? (
                      <span className="game-row-ready">Ready</span>
                    ) : null}
                  </button>
                ))
              )}
            </div>
            <div className="scan-row">
              <input
                className="ctrl-input"
                value={pathInput}
                onChange={(e) => onPathInput(e.target.value)}
                placeholder="Or paste game install folder…"
                spellCheck={false}
                disabled={busy}
                onKeyDown={(e) => {
                  if (e.key === "Enter") onScanPath();
                }}
              />
              <button type="button" className="btn-secondary" disabled={busy} onClick={onScanPath}>
                Scan
              </button>
            </div>
            <div className="start-onboard-actions">
              <button type="button" className="btn-secondary" disabled={busy} onClick={onBackToProjects}>
                Back
              </button>
              <button
                type="button"
                className="btn-primary"
                disabled={busy || !gameLoaded}
                onClick={onOpenEditor}
              >
                Continue to editor
              </button>
            </div>
          </section>
        </>
      ) : (
        <>
          <header className="ws-header">
            <div>
              <h1 className="ws-title">Start</h1>
              <p className="ws-sub">Create or open a mod project first.</p>
            </div>
          </header>

          <section className="start-panel">
            <p className="sb-label">Mod project</p>
            <p className="ide-muted">Create under examples/, or open any folder that contains mod.lua.</p>
            <div className="start-row">
              <input
                className="ctrl-input"
                value={projectName}
                onChange={(e) => onProjectName(e.target.value)}
                placeholder="Project name"
                disabled={busy}
              />
              <button type="button" className="btn-primary" disabled={busy} onClick={onNewProject}>
                New project
              </button>
            </div>
            <div className="start-row">
              <input
                className="ctrl-input"
                value={projectFolder}
                onChange={(e) => onProjectFolder(e.target.value)}
                placeholder="Paste project folder (must contain mod.lua)…"
                spellCheck={false}
                disabled={busy}
                onKeyDown={(e) => {
                  if (e.key === "Enter") onOpenFolder();
                }}
              />
              <button type="button" className="btn-secondary" disabled={busy} onClick={onOpenFolder}>
                Open folder
              </button>
            </div>
            {pipelines.length > 0 ? (
              <div className="start-examples">
                {pipelines.map((p) => (
                  <button
                    key={p.id}
                    type="button"
                    className="project-tile"
                    disabled={busy}
                    onClick={() => onOpenPipeline(p.id)}
                    onContextMenu={(e) => {
                      open(
                        e,
                        [
                          { id: "open", label: "Open" },
                          { id: "reveal", label: "Show in Explorer" },
                          { id: "sep", label: "", separator: true },
                          { id: "delete", label: "Delete Project…", danger: true },
                        ],
                        (id) => {
                          if (id === "open") onOpenPipeline(p.id);
                          else if (id === "reveal") onRevealPipeline(p.id);
                          else if (id === "delete") onDeletePipeline(p.id, p.name || p.id);
                        },
                      );
                    }}
                    title={p.path}
                  >
                    {p.name}
                  </button>
                ))}
              </div>
            ) : null}
          </section>

          <section className="start-howto">
            <p className="sb-label">How to edit something</p>
            <ol className="start-howto-list">
              <li>Open or create a project, then load your game.</li>
              <li>
                In <strong>Edit Mod</strong>, search Assets (e.g. <code>health</code>, <code>xp</code>).
              </li>
              <li>Preview the JSON → Insert snippet into <code>mod.lua</code> → change the value → Build.</li>
            </ol>
            <p className="ide-muted">
              Want more health? Search <code>health</code> in Assets, open a hit, copy the field name from
              Preview into <code>:field(...):set(...)</code>. See examples{" "}
              <code>morexp</code> / <code>250cap</code>, and <code>docs/README.md</code>.
            </p>
          </section>
        </>
      )}
    </div>
  );
}
