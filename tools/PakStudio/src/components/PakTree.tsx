"use client";

import { useCallback, useRef } from "react";
import { useContextMenu } from "@/components/ContextMenuProvider";
import type { PakEntry } from "@/lib/types";
import { displayName } from "@/lib/pak-tree";

type Props = {
  model: Map<string, PakEntry[]>;
  folderPath: string;
  selectedPath: string | null;
  onSelect: (entry: PakEntry) => void;
  emptyMessage?: string;
  onNearEnd?: () => void;
  hasMore?: boolean;
  onInsertSnippet?: (entry: PakEntry) => void;
  onCopyPath?: (entry: PakEntry) => void;
};

export function PakTree({
  model,
  folderPath,
  selectedPath,
  onSelect,
  emptyMessage = "No files in this view.",
  onNearEnd,
  hasMore = false,
  onInsertSnippet,
  onCopyPath,
}: Props) {
  const { open } = useContextMenu();
  const loadingMore = useRef(false);

  const onScroll = useCallback(
    (e: React.UIEvent<HTMLDivElement>) => {
      if (!onNearEnd || !hasMore || loadingMore.current) return;
      const el = e.currentTarget;
      if (el.scrollTop + el.clientHeight < el.scrollHeight - 64) return;
      loadingMore.current = true;
      onNearEnd();
      requestAnimationFrame(() => {
        loadingMore.current = false;
      });
    },
    [onNearEnd, hasMore],
  );

  if (model.size === 0) {
    return <p className="tree-empty">{emptyMessage}</p>;
  }

  return (
    <div className="tree-list" onScroll={onScroll}>
      {Array.from(model.entries()).map(([pak, items]) => (
        <div key={pak} className="tree-group">
          {model.size > 1 ? <div className="tree-pak">{pak}</div> : null}
          {items.map((entry) => {
            const selected = selectedPath === entry.virtualPath;
            return (
              <button
                key={`${entry.sourcePak}:${entry.virtualPath}`}
                type="button"
                title={`${entry.sourcePak} · ${entry.virtualPath}`}
                className={`tree-item ${selected ? "tree-item-selected" : ""}`}
                onClick={() => onSelect(entry)}
                onContextMenu={(e) => {
                  onSelect(entry);
                  open(
                    e,
                    [
                      ...(onInsertSnippet
                        ? [{ id: "insert", label: "Insert snippet" }]
                        : []),
                      { id: "copy-path", label: "Copy path" },
                      { id: "copy-name", label: "Copy file name" },
                    ],
                    (id) => {
                      if (id === "insert") onInsertSnippet?.(entry);
                      else if (id === "copy-path") {
                        if (onCopyPath) onCopyPath(entry);
                        else void navigator.clipboard.writeText(entry.virtualPath);
                      } else if (id === "copy-name") {
                        const name = entry.virtualPath.split("/").pop() ?? entry.virtualPath;
                        void navigator.clipboard.writeText(name);
                      }
                    },
                  );
                }}
              >
                <span className="tree-item-name">{displayName(entry, folderPath)}</span>
                <span className="tree-item-ext">{entry.extension || "?"}</span>
              </button>
            );
          })}
        </div>
      ))}
      {hasMore ? <p className="tree-load-hint">Scroll for more…</p> : null}
    </div>
  );
}
