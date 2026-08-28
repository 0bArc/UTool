"use client";

import { useMemo } from "react";
import { useContextMenu } from "@/components/ContextMenuProvider";
import {
  IconCheck,
  IconCopy,
  IconExternal,
  IconFile,
  IconPlay,
  IconX,
} from "@/components/icons";
import type { PipelineInfo } from "@/lib/pipelines-client";

export type BuildRecord = {
  id: string;
  name: string;
  path: string;
  ok: boolean;
  at: string;
  durationMs: number;
};

type Props = {
  pipeline: PipelineInfo | null;
  gameLabel: string;
  busy: boolean;
  compress: boolean;
  onCompressChange: (v: boolean) => void;
  forceExtract: boolean;
  onForceExtractChange: (v: boolean) => void;
  outputPath: string;
  onOutputPathChange: (v: string) => void;
  buildLog: string;
  buildOk: boolean | null;
  lastOutputPak: string;
  durationMs: number | null;
  history: BuildRecord[];
  onBuild: () => void;
  onDeploy: () => void;
  onCopyPath: () => void;
  onOpenFolder: () => void;
  onClearLog: () => void;
};

function formatDuration(ms: number): string {
  return `${(ms / 1000).toFixed(2)}s`;
}

function formatBuildAt(at: string): string {
  const d = new Date(at);
  if (Number.isNaN(d.getTime())) return at;
  return d.toLocaleString(undefined, {
    month: "short",
    day: "numeric",
    hour: "numeric",
    minute: "2-digit",
  });
}

function logLineClass(line: string): string {
  const l = line.toLowerCase();
  if (l.includes("error") || l.includes("failed") || l.includes("did not produce")) return "log-err";
  if (l.includes("warning")) return "log-warn";
  if (l.includes("built mod") || l.includes("deployed ") || l.includes("prepared "))
    return "log-ok";
  return "";
}

function parseStats(log: string, ok: boolean | null): { files: string; errors: number; warnings: number } {
  const prepared = log.match(/prepared (\d+) file/i);
  const errors = (log.match(/error/gi) ?? []).length && ok === false ? 1 : 0;
  const warnings = (log.match(/warning/gi) ?? []).length;
  return {
    files: prepared?.[1] ?? "0",
    errors,
    warnings,
  };
}

export function BuildView({
  pipeline,
  gameLabel,
  busy,
  compress,
  onCompressChange,
  forceExtract,
  onForceExtractChange,
  outputPath,
  onOutputPathChange,
  buildLog,
  buildOk,
  lastOutputPak,
  durationMs,
  history,
  onBuild,
  onDeploy,
  onCopyPath,
  onOpenFolder,
  onClearLog,
}: Props) {
  const lines = useMemo(() => (buildLog ? buildLog.split(/\r?\n/) : []), [buildLog]);
  const stats = parseStats(buildLog, buildOk);
  const pakName = lastOutputPak.replace(/^.*[/\\]/, "") || "";
  const pakFolder = lastOutputPak.replace(/[/\\][^/\\]+$/, "");
  const progress = busy ? 55 : buildOk === true ? 100 : buildOk === false ? 38 : 0;
  const gameName = gameLabel.replace(/\s*\(.*\)\s*$/, "") || "your game";
  const { open } = useContextMenu();

  const exportLog = () => {
    const blob = new Blob([buildLog || ""], { type: "text/plain" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = `${pipeline?.id ?? "build"}-log.txt`;
    a.click();
    URL.revokeObjectURL(url);
  };

  const statusTitle = busy
    ? "Working…"
    : buildOk === true
      ? "Completed"
      : buildOk === false
        ? "Failed"
        : "Ready to build";

  const statusSub = busy
    ? "Running utool…"
    : buildOk === true
      ? "Mod pak is ready."
      : buildOk === false
        ? "Check the log below."
        : pipeline
          ? `${pipeline.id} · ${gameLabel || gameName}`
          : "Select a project first.";

  return (
    <div
      className="ws-main build-hub"
      onContextMenu={(e) => {
        open(
          e,
          [
            { id: "build", label: busy ? "Building…" : "Build Mod", disabled: busy || !pipeline },
            { id: "deploy", label: busy ? "Busy…" : "Deploy", disabled: busy || !pipeline },
            { id: "sep1", label: "", separator: true },
            { id: "clear", label: "Clear log", disabled: !buildLog },
            { id: "export", label: "Export log", disabled: !buildLog },
            { id: "sep2", label: "", separator: true },
            { id: "copy", label: "Copy output path", disabled: !lastOutputPak },
            { id: "open", label: "Open output folder", disabled: !lastOutputPak },
          ],
          (id) => {
            if (id === "build") onBuild();
            else if (id === "deploy") onDeploy();
            else if (id === "clear") onClearLog();
            else if (id === "export") exportLog();
            else if (id === "copy") onCopyPath();
            else if (id === "open") onOpenFolder();
          },
        );
      }}
    >
      <header className="ws-header build-hub-header">
        <div>
          <h1 className="ws-title">Build</h1>
          <p className="ws-sub">Package your mod{gameName ? ` for ${gameName}` : ""}.</p>
        </div>
        <div className="ws-header-actions">
          <button
            type="button"
            className="btn-primary btn-build"
            disabled={busy || !pipeline}
            onClick={onBuild}
          >
            <IconPlay size={14} />
            {busy ? "Working…" : "Build"}
          </button>
          <button
            type="button"
            className="btn-secondary"
            disabled={busy || !pipeline}
            onClick={onDeploy}
            title="Build and copy *_P.pak into Content/Paks/mods"
          >
            Deploy
          </button>
          <button type="button" className="btn-secondary" disabled={!lastOutputPak} onClick={onOpenFolder}>
            Open output
          </button>
        </div>
      </header>

      <section className="build-block">
        <p className="sb-label">Configuration</p>
        <div className="start-row">
          <input className="ctrl-input" value={gameLabel || "from mod.lua"} readOnly title="Target game" />
        </div>
        <div className="start-row">
          <input
            className="ctrl-input"
            value={outputPath}
            onChange={(e) => onOutputPathChange(e.target.value)}
            placeholder="Output path (-o), optional"
            spellCheck={false}
            disabled={busy}
          />
        </div>
        <div className="build-checks">
          <label className="cfg-check">
            <input
              type="checkbox"
              checked={compress}
              onChange={(e) => onCompressChange(e.target.checked)}
              disabled={busy}
            />
            Compress
          </label>
          <label className="cfg-check">
            <input
              type="checkbox"
              checked={forceExtract}
              onChange={(e) => onForceExtractChange(e.target.checked)}
              disabled={busy}
            />
            Force extract
          </label>
        </div>
      </section>

      <section
        className={`build-status ${
          busy ? "result-busy" : buildOk === true ? "result-ok" : buildOk === false ? "result-fail" : ""
        }`}
      >
        <div className="build-status-top">
          <span className="build-status-icon" aria-hidden>
            {buildOk === false ? <IconX size={14} /> : <IconCheck size={14} />}
          </span>
          <div className="build-status-copy">
            <p className="build-status-title">{statusTitle}</p>
            <p className="build-status-sub">{statusSub}</p>
          </div>
          {durationMs != null ? <span className="result-time">{formatDuration(durationMs)}</span> : null}
        </div>
        <div className="progress build-progress">
          <div className="progress-fill" style={{ width: `${progress}%` }} />
        </div>
        <div className="build-stats">
          <span>{stats.files} files</span>
          <span className={stats.errors > 0 ? "stat-err" : ""}>{stats.errors} errors</span>
          <span className={stats.warnings > 0 ? "stat-warn" : ""}>{stats.warnings} warnings</span>
        </div>
      </section>

      <section className="build-log">
        <header className="build-log-head">
          <p className="sb-label">Log</p>
          <div className="log-actions">
            <button type="button" className="btn-secondary btn-sm" onClick={onClearLog}>
              Clear
            </button>
            <button type="button" className="btn-secondary btn-sm" onClick={exportLog} disabled={!buildLog}>
              Export
            </button>
          </div>
        </header>
        <pre className="build-log-body">
          {lines.length === 0
            ? "No build yet."
            : lines.map((line, i) => (
                <span key={i} className={logLineClass(line)}>
                  {line}
                  {"\n"}
                </span>
              ))}
        </pre>
      </section>

      <section className="build-out">
        <div className="out-icon" aria-hidden>
          <IconFile size={16} />
        </div>
        <div className="out-copy">
          <p className="out-name">{pakName || "Pak output"}</p>
          <p className="out-path" title={pakFolder}>
            {pakFolder || "Appears after a successful build."}
          </p>
        </div>
        <div className="out-actions">
          <button type="button" className="btn-icon" disabled={!lastOutputPak} onClick={onCopyPath} title="Copy path">
            <IconCopy size={16} />
          </button>
          <button
            type="button"
            className="btn-icon"
            disabled={!lastOutputPak}
            onClick={onOpenFolder}
            title="Open folder"
          >
            <IconExternal size={16} />
          </button>
        </div>
      </section>

      <section className="build-hist">
        <p className="sb-label">Previous builds</p>
        {history.length === 0 ? (
          <p className="sb-muted hist-empty">No builds yet.</p>
        ) : (
          <div className="hist-table">
            {history.slice(0, 8).map((h) => (
              <div key={h.id} className="hist-row">
                <span title={h.path}>{h.name}</span>
                <span>{formatBuildAt(h.at)}</span>
                <span className="hist-status">
                  <span className={`sb-dot ${h.ok ? "sb-dot-ok" : "sb-dot-err"}`} />
                  {h.ok ? "Ok" : "Fail"}
                </span>
              </div>
            ))}
          </div>
        )}
      </section>
    </div>
  );
}
