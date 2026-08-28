"use client";

import { useMemo, useState } from "react";
import type { FolderNode } from "@/lib/pak-tree";

type PakGroup = { name: string; count: number };

type Props = {
  folders: FolderNode[];
  paks: PakGroup[];
  selectedFolder: string;
  selectedPak: string;
  totalCount: number;
  onSelectFolder: (path: string) => void;
  onSelectPak: (pak: string) => void;
  hidePaks?: boolean;
};

function FolderRow({
  node,
  depth,
  selected,
  expanded,
  onToggle,
  onSelect,
}: {
  node: FolderNode;
  depth: number;
  selected: string;
  expanded: Set<string>;
  onToggle: (path: string) => void;
  onSelect: (path: string) => void;
}) {
  const isOpen = expanded.has(node.path);
  const isSelected = selected === node.path;
  const hasChildren = node.children.length > 0;

  return (
    <>
      <div className="cat-row" style={{ paddingLeft: `${0.5 + depth * 0.75}rem` }}>
        {hasChildren ? (
          <button
            type="button"
            className="cat-toggle"
            aria-label={isOpen ? "Collapse" : "Expand"}
            onClick={() => onToggle(node.path)}
          >
            {isOpen ? "−" : "+"}
          </button>
        ) : (
          <span className="cat-toggle-spacer" />
        )}
        <button
          type="button"
          className={`cat-item ${isSelected ? "cat-item-active" : ""}`}
          onClick={() => onSelect(node.path)}
          title={node.path}
        >
          <span className="cat-name">{node.name}</span>
          <span className="cat-count">{node.count}</span>
        </button>
      </div>
      {hasChildren && isOpen
        ? node.children.map((child) => (
            <FolderRow
              key={child.path}
              node={child}
              depth={depth + 1}
              selected={selected}
              expanded={expanded}
              onToggle={onToggle}
              onSelect={onSelect}
            />
          ))
        : null}
    </>
  );
}

export function CategorySidebar({
  folders,
  paks,
  selectedFolder,
  selectedPak,
  totalCount,
  onSelectFolder,
  onSelectPak,
  hidePaks = false,
}: Props) {
  const [expanded, setExpanded] = useState<Set<string>>(() => new Set());

  const toggle = (path: string) => {
    setExpanded((prev) => {
      const next = new Set(prev);
      if (next.has(path)) next.delete(path);
      else next.add(path);
      return next;
    });
  };

  const topFolders = useMemo(() => folders, [folders]);

  return (
    <aside className="studio-sidebar">
      {!hidePaks ? (
        <>
          <p className="sidebar-label">Paks</p>
          <button
            type="button"
            className={`cat-item cat-all ${selectedPak === "" ? "cat-item-active" : ""}`}
            onClick={() => onSelectPak("")}
          >
            <span className="cat-name">All paks</span>
            <span className="cat-count">{totalCount}</span>
          </button>
          {paks.map(({ name, count }) => (
            <button
              key={name}
              type="button"
              className={`cat-item ${selectedPak === name ? "cat-item-active" : ""}`}
              onClick={() => onSelectPak(name)}
              title={name}
            >
              <span className="cat-name">{name.replace(/\.pak$/i, "")}</span>
              <span className="cat-count">{count}</span>
            </button>
          ))}
        </>
      ) : null}

      <p className={`sidebar-label ${hidePaks ? "" : "sidebar-label-spaced"}`}>Folders</p>
      <button
        type="button"
        className={`cat-item cat-all ${selectedFolder === "" ? "cat-item-active" : ""}`}
        onClick={() => onSelectFolder("")}
      >
        <span className="cat-name">All paths</span>
        <span className="cat-count">{totalCount}</span>
      </button>
      {topFolders.map((node) => (
        <FolderRow
          key={node.path}
          node={node}
          depth={0}
          selected={selectedFolder}
          expanded={expanded}
          onToggle={toggle}
          onSelect={onSelectFolder}
        />
      ))}
    </aside>
  );
}
