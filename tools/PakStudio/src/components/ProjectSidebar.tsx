"use client";

import { useMemo, useState } from "react";
import type { ContextMenuItem } from "@/components/ContextMenu";
import { useContextMenu } from "@/components/ContextMenuProvider";
import { IconFile, IconFolder } from "@/components/icons";
import { buildProjectFileTree, type FileTreeNode } from "@/lib/file-tree";
import type { PipelineInfo } from "@/lib/pipelines-client";

type MenuTarget =
  | { kind: "blank" }
  | { kind: "file"; path: string }
  | { kind: "dir"; path: string };

type Props = {
  gameLabel: string;
  gamePath: string;
  gameLoaded: boolean;
  activeProject: PipelineInfo | null;
  selectedFile: string;
  onSelectFile: (path: string) => void;
  onChangeGame: () => void;
  onSwitchProject: () => void;
  busy: boolean;
  onNewProject: () => void;
  onOpenFolder: () => void;
  onNewFile: (baseDir?: string) => void;
  onNewFolder: (baseDir?: string) => void;
  onDeletePath: (path: string, isDir: boolean) => void;
};

function FileRow({
  node,
  depth,
  selectedFile,
  expanded,
  onToggle,
  onSelectFile,
  onContextMenu,
}: {
  node: FileTreeNode;
  depth: number;
  selectedFile: string;
  expanded: Set<string>;
  onToggle: (path: string) => void;
  onSelectFile: (path: string) => void;
  onContextMenu: (e: React.MouseEvent, target: MenuTarget) => void;
}) {
  const isDir = node.kind === "dir";
  const open = expanded.has(node.path);

  return (
    <>
      <button
        type="button"
        className={`file-tree-item ${!isDir && selectedFile === node.path ? "file-tree-item-active" : ""}`}
        style={{ paddingLeft: `${10 + depth * 12}px` }}
        onClick={() => {
          if (isDir) onToggle(node.path);
          else onSelectFile(node.path);
        }}
        onContextMenu={(e) =>
          onContextMenu(e, isDir ? { kind: "dir", path: node.path } : { kind: "file", path: node.path })
        }
        title={node.path}
      >
        {isDir ? (
          <span className="file-tree-chev">{open ? "▾" : "▸"}</span>
        ) : (
          <span className="file-tree-chev" />
        )}
        {isDir ? <IconFolder size={14} /> : <IconFile size={14} />}
        <span className="sb-item-name">{node.name}</span>
      </button>
      {isDir && open
        ? (node.children ?? []).map((child) => (
            <FileRow
              key={child.path}
              node={child}
              depth={depth + 1}
              selectedFile={selectedFile}
              expanded={expanded}
              onToggle={onToggle}
              onSelectFile={onSelectFile}
              onContextMenu={onContextMenu}
            />
          ))
        : null}
    </>
  );
}

function menuItemsFor(target: MenuTarget, hasProject: boolean): ContextMenuItem[] {
  if (!hasProject) {
    return [
      { id: "new-project", label: "New Project…" },
      { id: "open-folder", label: "Open Folder…" },
    ];
  }
  if (target.kind === "blank") {
    return [
      { id: "new-file", label: "New File…" },
      { id: "new-folder", label: "New Folder…" },
      { id: "sep", label: "", separator: true },
      { id: "new-project", label: "New Project…" },
      { id: "open-folder", label: "Open Folder…" },
    ];
  }
  if (target.kind === "dir") {
    return [
      { id: "new-file", label: "New File…" },
      { id: "new-folder", label: "New Folder…" },
      { id: "sep", label: "", separator: true },
      { id: "delete", label: "Delete Folder…", danger: true },
    ];
  }
  const locked = target.path === "mod.lua";
  return [
    { id: "new-file", label: "New File…" },
    { id: "new-folder", label: "New Folder…" },
    { id: "sep", label: "", separator: true },
    { id: "delete", label: "Delete File…", danger: true, disabled: locked },
  ];
}

export function ProjectSidebar({
  gameLabel,
  gamePath,
  gameLoaded,
  activeProject,
  selectedFile,
  onSelectFile,
  onChangeGame,
  onSwitchProject,
  busy,
  onNewProject,
  onOpenFolder,
  onNewFile,
  onNewFolder,
  onDeletePath,
}: Props) {
  const { open } = useContextMenu();
  const tree = useMemo(() => buildProjectFileTree(activeProject?.files ?? []), [activeProject?.files]);
  const [expanded, setExpanded] = useState<Set<string>>(() => new Set(["scripts", "dist"]));

  const toggle = (path: string) => {
    setExpanded((prev) => {
      const next = new Set(prev);
      if (next.has(path)) next.delete(path);
      else next.add(path);
      return next;
    });
  };

  const parentDir = (path: string) => {
    const i = path.lastIndexOf("/");
    return i >= 0 ? path.slice(0, i) : "";
  };

  const openContext = (e: React.MouseEvent, target: MenuTarget) => {
    const t = target;
    open(e, menuItemsFor(t, !!activeProject), (id) => {
      if (id === "new-project") onNewProject();
      else if (id === "open-folder") onOpenFolder();
      else if (id === "new-file") {
        const base = t.kind === "dir" ? t.path : t.kind === "file" ? parentDir(t.path) : undefined;
        onNewFile(base);
      } else if (id === "new-folder") {
        const base = t.kind === "dir" ? t.path : t.kind === "file" ? parentDir(t.path) : undefined;
        onNewFolder(base);
      } else if (id === "delete" && (t.kind === "file" || t.kind === "dir")) {
        onDeletePath(t.path, t.kind === "dir");
      }
    });
  };

  return (
    <aside className="ide-sidebar" onContextMenu={(e) => openContext(e, { kind: "blank" })}>
      <div className="ide-sidebar-scroll">
        <section className="sb-section">
          <p className="sb-label">Game</p>
          <div className="sb-game">
            <p className="sb-game-title">{gameLabel || "Not configured"}</p>
            {gamePath ? (
              <p className="sb-game-path" title={gamePath}>
                {gamePath}
              </p>
            ) : (
              <p className="sb-game-hint">Select a game from Start</p>
            )}
            {gameLoaded ? <p className="sb-game-ready">Inventory loaded</p> : null}
          </div>
          <button type="button" className="btn-secondary sb-change" onClick={onChangeGame}>
            Change game
          </button>
        </section>

        <section className="sb-section">
          <div className="sb-label-row">
            <p className="sb-label">Explorer</p>
            <div className="sb-icon-actions">
              <button type="button" className="sb-icon-btn" title="New project" disabled={busy} onClick={onNewProject}>
                +
              </button>
              <button type="button" className="sb-icon-btn" title="Open folder" disabled={busy} onClick={onOpenFolder}>
                Open
              </button>
            </div>
          </div>
          {!activeProject ? (
            <p className="sb-muted">No folder opened</p>
          ) : (
            <>
              <button
                type="button"
                className="explorer-root"
                onClick={onSwitchProject}
                onContextMenu={(e) => openContext(e, { kind: "blank" })}
                title={activeProject.path}
              >
                {activeProject.name || activeProject.id}
              </button>
              <div className="sb-icon-actions explorer-file-actions">
                <button
                  type="button"
                  className="sb-icon-btn"
                  title="New file"
                  disabled={busy}
                  onClick={() => onNewFile()}
                >
                  File
                </button>
                <button
                  type="button"
                  className="sb-icon-btn"
                  title="New folder"
                  disabled={busy}
                  onClick={() => onNewFolder()}
                >
                  Dir
                </button>
              </div>
              <div className="explorer-tree">
                {tree.length === 0 ? (
                  <p className="sb-muted">Empty project</p>
                ) : (
                  tree.map((node) => (
                    <FileRow
                      key={node.path}
                      node={node}
                      depth={1}
                      selectedFile={selectedFile}
                      expanded={expanded}
                      onToggle={toggle}
                      onSelectFile={onSelectFile}
                      onContextMenu={openContext}
                    />
                  ))
                )}
              </div>
            </>
          )}
        </section>
      </div>
    </aside>
  );
}
