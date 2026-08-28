"use client";

import Editor, { type OnMount } from "@monaco-editor/react";
import { useCallback, useEffect, useRef, useState } from "react";
import { fetchPipelineFile, savePipelineFile } from "@/lib/pipelines-client";
import { definePakStudioTheme } from "@/lib/monaco-theme";
import { registerUtoolLua } from "@/lib/utool-monaco";

type Props = {
  pipelineId: string;
  openFiles: string[];
  activeFile: string;
  onSelectFile: (path: string) => void;
  onCloseFile: (path: string) => void;
  insertText: string | null;
  onInsertConsumed: () => void;
};

function languageFor(path: string): string {
  if (path.endsWith(".json")) return "json";
  if (path.endsWith(".md")) return "markdown";
  return "lua";
}

function tabLabel(path: string): string {
  const parts = path.split("/");
  return parts[parts.length - 1] || path;
}

export function ModEditor({
  pipelineId,
  openFiles,
  activeFile,
  onSelectFile,
  onCloseFile,
  insertText,
  onInsertConsumed,
}: Props) {
  const [value, setValue] = useState("");
  const [saved, setSaved] = useState("");
  const [error, setError] = useState("");
  const [busy, setBusy] = useState(false);
  const editorRef = useRef<Parameters<OnMount>[0] | null>(null);
  const filePath = activeFile;

  useEffect(() => {
    if (!pipelineId || !filePath) {
      setValue("");
      setSaved("");
      return;
    }
    let cancelled = false;
    setBusy(true);
    setError("");
    void fetchPipelineFile(pipelineId, filePath)
      .then((content) => {
        if (cancelled) return;
        setValue(content);
        setSaved(content);
      })
      .catch((err) => {
        if (cancelled) return;
        setError(String(err));
        setValue("");
        setSaved("");
      })
      .finally(() => {
        if (!cancelled) setBusy(false);
      });
    return () => {
      cancelled = true;
    };
  }, [pipelineId, filePath]);

  useEffect(() => {
    if (!insertText || !editorRef.current) return;
    const editor = editorRef.current;
    const selection = editor.getSelection();
    if (selection) {
      editor.executeEdits("insert-snippet", [
        {
          range: selection,
          text: insertText.endsWith("\n") ? insertText : `${insertText}\n`,
          forceMoveMarkers: true,
        },
      ]);
    } else {
      const model = editor.getModel();
      if (model) {
        const end = model.getFullModelRange().getEndPosition();
        editor.executeEdits("insert-snippet", [
          {
            range: {
              startLineNumber: end.lineNumber,
              startColumn: end.column,
              endLineNumber: end.lineNumber,
              endColumn: end.column,
            },
            text: `\n${insertText}\n`,
            forceMoveMarkers: true,
          },
        ]);
      }
    }
    editor.focus();
    onInsertConsumed();
  }, [insertText, onInsertConsumed]);

  const dirty = value !== saved;

  const save = useCallback(async () => {
    if (!pipelineId || !filePath) return;
    setBusy(true);
    setError("");
    try {
      await savePipelineFile(pipelineId, filePath, value);
      setSaved(value);
    } catch (err) {
      setError(String(err));
    } finally {
      setBusy(false);
    }
  }, [pipelineId, filePath, value]);

  const onMount: OnMount = (editor) => {
    editorRef.current = editor;
  };

  if (!pipelineId) {
    return (
      <div className="mod-editor empty">
        <p>Create or open a project to edit mod.lua</p>
      </div>
    );
  }

  return (
    <div className="mod-editor">
      <div className="editor-tabs">
        {openFiles.length === 0 ? (
          <span className="editor-tab-empty">No files open</span>
        ) : (
          openFiles.map((f) => (
            <div key={f} className={`editor-tab ${f === filePath ? "editor-tab-active" : ""}`}>
              <button type="button" className="editor-tab-btn" onClick={() => onSelectFile(f)} title={f}>
                {tabLabel(f)}
                {f === filePath && dirty ? " *" : ""}
              </button>
              <button
                type="button"
                className="editor-tab-close"
                title="Close"
                onClick={(e) => {
                  e.stopPropagation();
                  onCloseFile(f);
                }}
              >
                ×
              </button>
            </div>
          ))
        )}
        <button
          type="button"
          className="btn-primary btn-sm editor-save"
          disabled={busy || !dirty || !filePath}
          onClick={() => void save()}
        >
          Save
        </button>
      </div>
      {error ? <p className="mod-editor-error">{error}</p> : null}
      <div className="mod-editor-body">
        {filePath ? (
          <Editor
            height="100%"
            theme="pak-studio"
            language={languageFor(filePath)}
            value={value}
            onChange={(v) => setValue(v ?? "")}
            beforeMount={(monaco) => {
              definePakStudioTheme(monaco);
              registerUtoolLua(monaco);
            }}
            onMount={onMount}
            options={{
              fontSize: 13,
              fontFamily: "JetBrains Mono, Consolas, monospace",
              minimap: { enabled: false },
              scrollBeyondLastLine: false,
              wordWrap: "on",
              automaticLayout: true,
              padding: { top: 8 },
              suggestOnTriggerCharacters: true,
              quickSuggestions: { other: true, strings: true, comments: false },
            }}
          />
        ) : (
          <div className="mod-editor empty">
            <p>Select a file from the explorer</p>
          </div>
        )}
      </div>
    </div>
  );
}
