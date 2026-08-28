"use client";

import { CategorySidebar } from "@/components/CategorySidebar";
import { Paginator } from "@/components/Paginator";
import { PakTree } from "@/components/PakTree";
import { PathSearch } from "@/components/PathSearch";
import type { FolderNode } from "@/lib/pak-tree";
import type { PakEntry } from "@/lib/types";

export type BrowseMode = "json" | "all";

type Props = {
  hasAssets: boolean;
  busy: boolean;
  browseMode: BrowseMode;
  onBrowseMode: (mode: BrowseMode) => void;
  search: string;
  onSearch: (v: string) => void;
  onSearchGo: () => void;
  pathSearchEntries: PakEntry[];
  folders: FolderNode[];
  paks: Array<{ name: string; count: number }>;
  folder: string;
  pak: string;
  sidebarCount: number;
  inventoryNote?: string;
  onSelectFolder: (path: string) => void;
  onSelectPak: (name: string) => void;
  tree: Map<string, PakEntry[]>;
  selected: PakEntry | null;
  onSelectEntry: (e: PakEntry) => void;
  onInsertSnippet?: (e: PakEntry) => void;
  safePage: number;
  totalPages: number;
  scopedCount: number;
  shownCount: number;
  onPageChange: (n: number) => void;
};

export function AssetSidebar({
  hasAssets,
  busy,
  browseMode,
  onBrowseMode,
  search,
  onSearch,
  onSearchGo,
  pathSearchEntries,
  folders,
  paks,
  folder,
  pak,
  sidebarCount,
  inventoryNote,
  onSelectFolder,
  onSelectPak,
  tree,
  selected,
  onSelectEntry,
  onInsertSnippet,
  safePage,
  totalPages,
  scopedCount,
  shownCount,
  onPageChange,
}: Props) {
  const hasMore = safePage < totalPages - 1;

  return (
    <aside className="ide-config asset-browser-pane">
      <div className="sb-label-row">
        <p className="sb-label">Assets</p>
        <div className="browse-mode-toggle">
          <button
            type="button"
            className={`browse-mode-btn ${browseMode === "json" ? "browse-mode-btn-active" : ""}`}
            disabled={busy}
            onClick={() => onBrowseMode("json")}
          >
            JSON
          </button>
          <button
            type="button"
            className={`browse-mode-btn ${browseMode === "all" ? "browse-mode-btn-active" : ""}`}
            disabled={busy}
            onClick={() => onBrowseMode("all")}
          >
            All
          </button>
        </div>
      </div>
      {!hasAssets ? (
        <p className="ide-muted">Load a game to browse pak contents.</p>
      ) : (
        <>
          <p className="json-panel-count text-caption">
            {sidebarCount.toLocaleString()} {browseMode === "json" ? "JSON tables" : "files"}
            {inventoryNote ? ` · ${inventoryNote}` : ""}
          </p>

          <div className="json-panel-folders">
            <CategorySidebar
              folders={folders}
              paks={paks}
              selectedFolder={search.trim() ? "" : folder}
              selectedPak={search.trim() ? "" : pak}
              totalCount={sidebarCount}
              onSelectFolder={onSelectFolder}
              onSelectPak={onSelectPak}
            />
          </div>

          <div className="json-panel-list">
            <div className="json-panel-list-search">
              <PathSearch
                value={search}
                onChange={onSearch}
                onPick={(path) => {
                  const last = path.split("/").pop() ?? "";
                  if (last && !last.includes(".")) onSelectFolder(path);
                }}
                onSubmit={onSearchGo}
                entries={pathSearchEntries}
                disabled={busy}
              />
            </div>
            <PakTree
              model={tree}
              folderPath={folder}
              selectedPath={selected?.virtualPath ?? null}
              onSelect={onSelectEntry}
              onInsertSnippet={onInsertSnippet}
              emptyMessage={
                search.trim()
                  ? "No matches."
                  : browseMode === "json"
                    ? "No JSON in this view."
                    : "No files in this view."
              }
              hasMore={hasMore}
              onNearEnd={() => {
                if (hasMore) onPageChange(safePage + 1);
              }}
            />
            <Paginator
              page={safePage}
              totalPages={totalPages}
              totalItems={scopedCount}
              shownCount={shownCount}
              onPageChange={onPageChange}
            />
          </div>
        </>
      )}
    </aside>
  );
}
