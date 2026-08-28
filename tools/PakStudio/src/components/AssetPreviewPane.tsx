"use client";

import Editor from "@monaco-editor/react";
import { useMemo } from "react";
import { useContextMenu } from "@/components/ContextMenuProvider";
import type { PakEntry } from "@/lib/types";
import { definePakStudioTheme } from "@/lib/monaco-theme";
import { extractPreviewBody } from "@/lib/preview";

type Props = {
  selected: PakEntry | null;
  preview: string;
  previewError: boolean;
  busy: boolean;
  formatBytes: (n: number) => string;
  onInsertSnippet: () => void;
  canInsert: boolean;
};

export function AssetPreviewPane({
  selected,
  preview,
  previewError,
  busy,
  formatBytes,
  onInsertSnippet,
  canInsert,
}: Props) {
  const { open } = useContextMenu();
  const previewDoc = useMemo(
    () => extractPreviewBody(preview, selected?.extension),
    [preview, selected?.extension],
  );

  return (
    <aside
      className="asset-preview-pane"
      onContextMenu={(e) => {
        open(
          e,
          [
            {
              id: "insert",
              label: "Insert snippet",
              disabled: !canInsert || previewError || !preview,
            },
            {
              id: "copy-path",
              label: "Copy path",
              disabled: !selected,
            },
          ],
          (id) => {
            if (id === "insert") onInsertSnippet();
            else if (id === "copy-path" && selected) {
              void navigator.clipboard.writeText(selected.virtualPath);
            }
          },
        );
      }}
    >
      <p className="sb-label">Preview</p>
      {!selected ? (
        <p className="ide-muted preview-empty-hint">Pick a file from the asset list.</p>
      ) : (
        <>
          <dl className="ide-meta ide-meta-compact">
            <div>
              <dt>Path</dt>
              <dd title={selected.virtualPath}>{selected.virtualPath}</dd>
            </div>
            <div>
              <dt>Type</dt>
              <dd>.{selected.extension || "?"}</dd>
            </div>
            <div>
              <dt>Size</dt>
              <dd>{formatBytes(selected.size)}</dd>
            </div>
            <div>
              <dt>Pak</dt>
              <dd>{selected.sourcePak.replace(/\.pak$/i, "")}</dd>
            </div>
          </dl>
          <div className="preview-pane-body">
            {busy && !preview ? (
              <p className="ide-muted">Loading…</p>
            ) : previewDoc.content ? (
              <div className={`preview-pane-editor ${previewError ? "preview-pane-error" : ""}`}>
                <Editor
                  height="100%"
                  theme="pak-studio"
                  language={previewDoc.language}
                  value={previewDoc.content}
                  beforeMount={definePakStudioTheme}
                  options={{
                    readOnly: true,
                    domReadOnly: true,
                    fontSize: 11,
                    fontFamily: "JetBrains Mono, Consolas, monospace",
                    minimap: { enabled: false },
                    scrollBeyondLastLine: false,
                    wordWrap: "on",
                    automaticLayout: true,
                    lineNumbers: "off",
                    folding: true,
                    renderLineHighlight: "none",
                    scrollbar: { vertical: "auto", horizontal: "auto" },
                    padding: { top: 8, bottom: 8 },
                    contextmenu: false,
                  }}
                />
              </div>
            ) : (
              <p className="ide-muted">No preview for this file type.</p>
            )}
          </div>
          <button
            type="button"
            className="btn-primary preview-insert"
            disabled={!canInsert || previewError || !preview}
            onClick={onInsertSnippet}
          >
            Insert snippet
          </button>
        </>
      )}
    </aside>
  );
}
