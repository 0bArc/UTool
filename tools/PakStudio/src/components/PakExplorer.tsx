"use client";

import { useCallback, useEffect, useMemo, useState } from "react";
import { AppRail } from "@/components/AppRail";
import { AssetPreviewPane } from "@/components/AssetPreviewPane";
import { AssetSidebar, type BrowseMode } from "@/components/AssetSidebar";
import { BuildView, type BuildRecord } from "@/components/BuildView";
import { ConfirmDialog, PromptDialog } from "@/components/Dialog";
import { useContextMenu } from "@/components/ContextMenuProvider";
import { ModEditor } from "@/components/ModEditor";
import { ProjectSidebar } from "@/components/ProjectSidebar";
import { SiteNav, type StudioView } from "@/components/SiteNav";
import { WelcomeView, type StartStep } from "@/components/WelcomeView";
import { loadBuildHistory, saveBuildHistory } from "@/lib/build-history";
import { loadStudioSession, saveStudioSession } from "@/lib/studio-session";
import {
  buildFolderTree,
  buildTreeModel,
  collectPakGroups,
  filterEntriesByExtension,
  filterEntriesByFolder,
  filterEntriesByPak,
  filterEntriesByQuery,
  searchMatchSummary,
} from "@/lib/pak-tree";
import { pageCount, sliceThroughPage } from "@/lib/paginate";
import { formatPreviewResponse, formatSnippetResponse } from "@/lib/preview";
import {
  createPipeline,
  createPipelineEntry,
  deletePipelineEntry,
  fetchPipelineFile,
  fetchPipelines,
  removePipeline,
  type PipelineInfo,
} from "@/lib/pipelines-client";
import { fetchGames, probeGamePath, type ConfigGame } from "@/lib/games-client";
import type { PakEntry, UtoolListResponse } from "@/lib/types";
import { utool } from "@/lib/utool-client";

type PromptKind = "new-project" | "open-folder" | "new-file" | "new-folder";

type PromptState = {
  kind: PromptKind;
  title: string;
  label: string;
  value: string;
  confirmLabel: string;
};

type ConfirmState =
  | { kind: "file"; path: string; isDir: boolean }
  | { kind: "project"; id: string; name: string };

function isBinaryProjectFile(path: string): boolean {
  const lower = path.toLowerCase();
  return lower.endsWith(".pak") || lower.endsWith(".zip");
}

function joinProjectPath(root: string, rel: string): string {
  const sep = root.includes("\\") ? "\\" : "/";
  return `${root.replace(/[/\\]+$/, "")}${sep}${rel.replace(/\//g, sep)}`;
}

function formatBytes(n: number): string {
  if (n < 1024) return `${n} B`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
  return `${(n / (1024 * 1024)).toFixed(1)} MB`;
}

import { parseBuildArtifact } from "@/lib/build-output";

function parseGameIdFromModLua(text: string): string | null {
  const m = text.match(/gameId\s*=\s*"([^"]+)"/);
  return m?.[1] ?? null;
}

async function openPath(path: string): Promise<void> {
  const res = await fetch("/api/open-path", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ path }),
  });
  const doc = (await res.json()) as { ok?: boolean; error?: string };
  if (!res.ok || !doc.ok) throw new Error(doc.error ?? "Could not open folder");
}

export function PakExplorer() {
  const [view, setView] = useState<StudioView>("welcome");
  const [projectName, setProjectName] = useState("");
  const [projectFolder, setProjectFolder] = useState("");
  const [games, setGames] = useState<ConfigGame[]>([]);
  const [source, setSource] = useState("");
  const [pathInput, setPathInput] = useState("");
  const [gameLabel, setGameLabel] = useState("");
  const [gamePath, setGamePath] = useState("");
  const [search, setSearch] = useState("");
  const [folder, setFolder] = useState("");
  const [pak, setPak] = useState("");
  const [browseMode, setBrowseMode] = useState<BrowseMode>("json");
  const [inventoryTotal, setInventoryTotal] = useState<number | null>(null);
  const [page, setPage] = useState(0);
  const [status, setStatus] = useState("Ready");
  const [statusError, setStatusError] = useState(false);
  const [allEntries, setAllEntries] = useState<PakEntry[]>([]);
  const [selected, setSelected] = useState<PakEntry | null>(null);
  const [preview, setPreview] = useState("");
  const [previewError, setPreviewError] = useState(false);
  const [busy, setBusy] = useState(false);
  const [deepResults, setDeepResults] = useState<PakEntry[] | null>(null);
  const [output, setOutput] = useState("Ready.");
  const [bottomTab, setBottomTab] = useState<"output" | "console">("output");

  const [pipelines, setPipelines] = useState<PipelineInfo[]>([]);
  const [pipelineId, setPipelineId] = useState("");
  const [openFiles, setOpenFiles] = useState<string[]>([]);
  const [activeFile, setActiveFile] = useState("");
  const [insertText, setInsertText] = useState<string | null>(null);

  const [compress, setCompress] = useState(false);
  const [forceExtract, setForceExtract] = useState(false);
  const [outputPath, setOutputPath] = useState("");
  const [buildLog, setBuildLog] = useState("");
  const [buildOk, setBuildOk] = useState<boolean | null>(null);
  const [lastOutputPak, setLastOutputPak] = useState("");
  const [durationMs, setDurationMs] = useState<number | null>(null);
  const [history, setHistory] = useState<BuildRecord[]>([]);
  const [historyReady, setHistoryReady] = useState(false);
  const [prompt, setPrompt] = useState<PromptState | null>(null);
  const [confirm, setConfirm] = useState<ConfirmState | null>(null);
  const [startStep, setStartStep] = useState<StartStep>("project");
  const [sessionReady, setSessionReady] = useState(false);
  const [pendingGameRestore, setPendingGameRestore] = useState<{
    src: string;
    label: string;
    path: string;
    mode: BrowseMode;
  } | null>(null);

  const hasAssets = allEntries.length > 0;
  const activePipeline = pipelines.find((p) => p.id === pipelineId) ?? null;

  const openFileTab = useCallback(
    (path: string) => {
      if (isBinaryProjectFile(path)) {
        if (activePipeline) {
          void openPath(joinProjectPath(activePipeline.path, path)).catch((err) => {
            setStatus(String(err));
            setStatusError(true);
          });
        }
        return;
      }
      setOpenFiles((prev) => (prev.includes(path) ? prev : [...prev, path]));
      setActiveFile(path);
      setView("editor");
    },
    [activePipeline],
  );

  useEffect(() => {
    setHistory(loadBuildHistory());
    setHistoryReady(true);
  }, []);

  useEffect(() => {
    if (!historyReady) return;
    saveBuildHistory(history);
  }, [history, historyReady]);

  useEffect(() => {
    let cancelled = false;
    void (async () => {
      const session = loadStudioSession();
      try {
        const [gamesDoc, list] = await Promise.all([fetchGames(), fetchPipelines()]);
        if (cancelled) return;
        const gameList = gamesDoc.games ?? [];
        setGames(gameList);
        setPipelines(list);

        if (session) {
          const pipelineOk = session.pipelineId
            ? list.some((p) => p.id === session.pipelineId)
            : false;
          if (pipelineOk) {
            setPipelineId(session.pipelineId);
            const pipe = list.find((p) => p.id === session.pipelineId);
            const files = new Set(pipe?.files ?? []);
            const restoredFiles = session.openFiles.filter((f) => files.has(f));
            const restoredActive =
              session.activeFile && files.has(session.activeFile)
                ? session.activeFile
                : restoredFiles[0] ?? (files.has("mod.lua") ? "mod.lua" : "");
            setOpenFiles(
              restoredFiles.length > 0
                ? restoredFiles
                : restoredActive
                  ? [restoredActive]
                  : [],
            );
            setActiveFile(restoredActive);
            setBrowseMode(session.browseMode);
            setStartStep(session.startStep);
            if (session.view === "editor" || session.view === "build") {
              setView(session.view);
            } else {
              setView("welcome");
            }

            if (session.gameSource) {
              const g = gameList.find((x: ConfigGame) => x.id === session.gameSource);
              setGameLabel(session.gameLabel || session.gameSource);
              setGamePath(session.gamePath || g?.paksDir || "");
              setSource(session.gameSource);
              setPendingGameRestore({
                src: session.gameSource,
                label: session.gameLabel || session.gameSource,
                path: session.gamePath || g?.paksDir || "",
                mode: session.browseMode,
              });
            }
          } else if (session.view === "welcome") {
            setView("welcome");
            setStartStep(session.startStep);
          }
        }
      } catch (err) {
        if (!cancelled) {
          setStatus(String(err));
          setStatusError(true);
        }
      } finally {
        if (!cancelled) setSessionReady(true);
      }
    })();
    return () => {
      cancelled = true;
    };
  }, []);

  useEffect(() => {
    if (!sessionReady) return;
    saveStudioSession({
      view,
      pipelineId,
      openFiles,
      activeFile,
      startStep,
      gameSource: source,
      gameLabel,
      gamePath,
      browseMode,
    });
  }, [
    sessionReady,
    view,
    pipelineId,
    openFiles,
    activeFile,
    startStep,
    source,
    gameLabel,
    gamePath,
    browseMode,
  ]);

  // games/pipelines fetch moved into session restore effect above

  const catalogEntries = useMemo(
    () => (browseMode === "json" ? filterEntriesByExtension(allEntries, "json") : allEntries),
    [allEntries, browseMode],
  );

  const scopedEntries = useMemo(() => {
    let entries = catalogEntries;
    const q = search.trim();
    if (deepResults) {
      entries = browseMode === "json" ? filterEntriesByExtension(deepResults, "json") : deepResults;
    } else if (q) {
      entries = filterEntriesByQuery(entries, q);
    } else {
      if (pak) entries = filterEntriesByPak(entries, pak);
      if (folder) entries = filterEntriesByFolder(entries, folder);
    }
    return entries;
  }, [catalogEntries, browseMode, deepResults, folder, pak, search]);

  const pathSearchEntries = useMemo(() => {
    const q = search.trim();
    if (!q) return catalogEntries;
    if (deepResults) {
      return browseMode === "json" ? filterEntriesByExtension(deepResults, "json") : deepResults;
    }
    return filterEntriesByQuery(catalogEntries, q);
  }, [catalogEntries, browseMode, deepResults, search]);

  const folderScopeEntries = useMemo(() => {
    if (search.trim()) return catalogEntries;
    if (pak) return filterEntriesByPak(catalogEntries, pak);
    return catalogEntries;
  }, [catalogEntries, pak, search]);

  const folders = useMemo(() => buildFolderTree(folderScopeEntries), [folderScopeEntries]);
  const paks = useMemo(() => collectPakGroups(catalogEntries), [catalogEntries]);
  const sidebarCount = scopedEntries.length;
  const inventoryNote =
    inventoryTotal != null && inventoryTotal > allEntries.length
      ? `showing ${allEntries.length} of ${inventoryTotal}`
      : undefined;
  const totalPages = pageCount(scopedEntries.length);
  const safePage = Math.min(page, Math.max(0, totalPages - 1));
  const pageEntries = useMemo(
    () => sliceThroughPage(scopedEntries, safePage),
    [scopedEntries, safePage],
  );
  const tree = useMemo(() => buildTreeModel(pageEntries), [pageEntries]);

  useEffect(() => {
    setPage(0);
  }, [search, folder, pak, catalogEntries, deepResults, browseMode]);

  const loadInventory = useCallback(
    async (
      src: string,
      label?: string,
      path?: string,
      mode: BrowseMode = browseMode,
      opts?: { preferEditor?: boolean },
    ) => {
      if (!src) return;
      setBusy(true);
      setStatus("Loading…");
      setStatusError(false);
      setFolder("");
      setPak("");
      setSearch("");
      setDeepResults(null);
      setSelected(null);
      setPreview("");
      try {
        const args =
          mode === "json"
            ? ["pak", "list", src, "--json", "--ext", "json", "--limit", "10000"]
            : ["pak", "list", src, "--json", "--limit", "10000"];
        const raw = await utool(args);
        const doc = JSON.parse(raw) as UtoolListResponse;
        const entries = doc.entries ?? [];
        const total = doc.total ?? entries.length;
        setAllEntries(entries);
        setInventoryTotal(total);
        setBrowseMode(mode);
        setSource(src);
        setGameLabel(label ?? src);
        if (path) setGamePath(path);
        const kind = mode === "json" ? "JSON tables" : "files";
        const msg = entries.length
          ? `Loaded ${entries.length} ${kind}${total > entries.length ? ` (${total} total in game)` : ""}`
          : `No ${kind} in ${label ?? src}. Check dataPak in utool.json.`;
        setStatus(entries.length ? "Ready" : "Empty");
        setStatusError(entries.length === 0);
        setOutput(msg);
        setBottomTab("output");
        if (opts?.preferEditor !== false && entries.length && pipelineId) setView("editor");
      } catch (err) {
        setAllEntries([]);
        setInventoryTotal(null);
        setStatus("Load failed");
        setStatusError(true);
        setOutput(String(err));
      } finally {
        setBusy(false);
      }
    },
    [browseMode, pipelineId],
  );

  useEffect(() => {
    if (!pendingGameRestore) return;
    const job = pendingGameRestore;
    setPendingGameRestore(null);
    void loadInventory(job.src, job.label, job.path, job.mode, { preferEditor: false });
  }, [pendingGameRestore, loadInventory]);

  const onBrowseMode = useCallback(
    (mode: BrowseMode) => {
      if (mode === browseMode) return;
      if (!source) {
        setBrowseMode(mode);
        return;
      }
      void loadInventory(source, gameLabel, gamePath, mode);
    },
    [browseMode, source, gameLabel, gamePath, loadInventory],
  );

  const pickGame = useCallback((g: ConfigGame) => {
    void loadInventory(
      g.id,
      `${g.id}${g.pakCount != null ? ` (${g.pakCount})` : ""}`,
      g.paksDir ?? "",
    );
  }, [loadInventory]);

  const autoSelectGameForPipeline = useCallback(
    async (pipeline: PipelineInfo, gameList: ConfigGame[]) => {
      if (gameList.length === 0) return;
      let gameId: string | null = null;
      try {
        if (pipeline.files.includes("mod.lua")) {
          const text = await fetchPipelineFile(pipeline.id, "mod.lua");
          gameId = parseGameIdFromModLua(text);
        }
      } catch {
        /* fall through */
      }
      const match =
        (gameId ? gameList.find((g) => g.id.toLowerCase() === gameId.toLowerCase()) : undefined) ??
        gameList.find((g) => g.id.toLowerCase() === "icarus") ??
        gameList[0];
      if (!match) return;
      if (source === match.id && allEntries.length > 0) return;
      pickGame(match);
    },
    [source, allEntries.length, pickGame],
  );

  const scanPath = async () => {
    const path = pathInput.trim();
    if (!path) return;
    setBusy(true);
    setStatus("Scanning…");
    try {
      const doc = await probeGamePath(path);
      if (!doc.ok || !doc.ready || !doc.source) {
        setStatus("Scan failed");
        setStatusError(true);
        setOutput(doc.error ?? "No paks found");
        return;
      }
      const label = doc.matchedGameId
        ? `${doc.matchedGameId} (${doc.pakCount ?? 0})`
        : `${doc.pakCount ?? 0} paks`;
      await loadInventory(doc.source, label, doc.paksDir ?? path);
    } catch (err) {
      setStatus("Scan failed");
      setStatusError(true);
      setOutput(String(err));
    } finally {
      setBusy(false);
    }
  };

  const selectEntry = useCallback(
    async (entry: PakEntry) => {
      if (!source) return;
      setSelected(entry);
      setBusy(true);
      try {
        const raw = await utool([
          "pak",
          "open",
          entry.virtualPath,
          "--from",
          source,
          "--pak",
          entry.sourcePak,
          "--json",
        ]);
        setPreview(formatPreviewResponse(JSON.parse(raw)));
        setPreviewError(false);
        setStatus("Ready");
        setStatusError(false);
        setOutput(`Opened ${entry.virtualPath}`);
      } catch (err) {
        setPreview(String(err));
        setPreviewError(true);
        setStatus("Open failed");
        setStatusError(true);
        setOutput(String(err));
      } finally {
        setBusy(false);
      }
    },
    [source],
  );

  const runServerSearch = useCallback(
    async (inside = false) => {
      const q = search.trim();
      if (!q || !source) return;
      if (!inside) {
        const local = filterEntriesByQuery(catalogEntries, q);
        setDeepResults(null);
        if (local.length > 0) {
          const hint = searchMatchSummary(local, q);
          setOutput(hint ? `${local.length} matches · ${hint}` : `${local.length} matches`);
          setStatus("Ready");
          return;
        }
      }
      setBusy(true);
      try {
        const args = ["pak", "search", q, "--from", source, "--json"];
        if (browseMode === "json") args.push("--ext", "json");
        if (inside) args.push("--inside");
        const raw = await utool(args);
        const doc = JSON.parse(raw) as UtoolListResponse;
        const entries = doc.entries ?? [];
        setDeepResults(inside ? entries : null);
        setFolder("");
        setPak("");
        setOutput(entries.length ? `${entries.length} matches` : "No matches");
        setStatus(entries.length ? "Ready" : "No matches");
        setStatusError(entries.length === 0);
      } catch (err) {
        setOutput(String(err));
        setStatus("Search failed");
        setStatusError(true);
      } finally {
        setBusy(false);
      }
    },
    [search, source, catalogEntries, browseMode],
  );

  const insertSnippet = useCallback(
    async (entryOverride?: PakEntry) => {
      const entry = entryOverride ?? selected;
      if (!entry || !source) return;
      if (!pipelineId) {
        setStatus("Open a project first");
        setStatusError(true);
        return;
      }
      try {
        setSelected(entry);
        const raw = await utool([
          "pak",
          "snippet",
          entry.virtualPath,
          "--from",
          source,
          "--pak",
          entry.sourcePak,
          "--json",
        ]);
        const snippet = formatSnippetResponse(JSON.parse(raw));
        if (!snippet) throw new Error("No snippet for this asset");
        const target = activeFile || openFiles[0] || "mod.lua";
        openFileTab(target);
        setInsertText(snippet);
        setOutput("Snippet inserted into editor");
        setStatus("Ready");
      } catch (err) {
        setOutput(String(err));
        setStatusError(true);
      }
    },
    [selected, source, pipelineId, activeFile, openFiles, openFileTab],
  );

  const buildMod = useCallback(async () => {
    if (!activePipeline) return;
    setBusy(true);
    setStatus("Building…");
    setBottomTab("output");
    setBuildOk(null);
    const started = Date.now();
    try {
      const args = ["pak", "build-mod", activePipeline.path];
      if (compress) args.push("-compress");
      if (forceExtract) args.push("--force-extract");
      const out = outputPath.trim();
      if (out) args.push("-o", out);
      const raw = await utool(args);
      const log = raw || `Build completed: ${activePipeline.id}`;
      const ms = Date.now() - started;
      const artifactOut = parseBuildArtifact(log);
      setOutput(log);
      setBuildLog(log);
      setLastOutputPak(artifactOut);
      setDurationMs(ms);
      setBuildOk(true);
      setStatus("Build ok");
      setStatusError(false);
      setHistory((prev) =>
        [
          {
            id: String(started),
            name: artifactOut.replace(/^.*[/\\]/, "") || activePipeline.id,
            path: artifactOut,
            ok: true,
            at: new Date(started).toISOString(),
            durationMs: ms,
          },
          ...prev,
        ].slice(0, 40),
      );
      try {
        const list = await fetchPipelines();
        setPipelines(list);
      } catch {
        /* keep current tree */
      }
    } catch (err) {
      const msg = String(err);
      const ms = Date.now() - started;
      setOutput(msg);
      setBuildLog(msg);
      setDurationMs(ms);
      setBuildOk(false);
      setStatus("Build failed");
      setStatusError(true);
      setHistory((prev) =>
        [
          {
            id: String(started),
            name: activePipeline.id,
            path: activePipeline.path,
            ok: false,
            at: new Date(started).toISOString(),
            durationMs: ms,
          },
          ...prev,
        ].slice(0, 40),
      );
    } finally {
      setBusy(false);
    }
  }, [activePipeline, compress, forceExtract, outputPath]);

  const deployMod = useCallback(async () => {
    if (!activePipeline) return;
    setBusy(true);
    setStatus("Deploying…");
    setBottomTab("output");
    setBuildOk(null);
    const started = Date.now();
    try {
      const raw = await utool(["deploy", activePipeline.path]);
      const log = raw || `Deploy completed: ${activePipeline.id}`;
      const ms = Date.now() - started;
      const artifactOut = parseBuildArtifact(log) || lastOutputPak;
      setOutput(log);
      setBuildLog(log);
      if (artifactOut) setLastOutputPak(artifactOut);
      setDurationMs(ms);
      setBuildOk(true);
      setStatus("Deploy ok");
      setStatusError(false);
      try {
        const list = await fetchPipelines();
        setPipelines(list);
      } catch {
        /* keep */
      }
    } catch (err) {
      const msg = String(err);
      const ms = Date.now() - started;
      setOutput(msg);
      setBuildLog(msg);
      setDurationMs(ms);
      setBuildOk(false);
      setStatus("Deploy failed");
      setStatusError(true);
    } finally {
      setBusy(false);
    }
  }, [activePipeline, lastOutputPak]);

  const applyPipeline = async (created: PipelineInfo, opts?: { onboard?: boolean }) => {
    const list = await fetchPipelines();
    setPipelines(list);
    setPipelineId(created.id);
    const first = created.files.includes("mod.lua") ? "mod.lua" : created.files[0] ?? "";
    setOpenFiles(first ? [first] : []);
    setActiveFile(first);
    setView("welcome");
    setStartStep(opts?.onboard ? "game" : "project");
    setStatus("Ready");
    setStatusError(false);
    await autoSelectGameForPipeline(created, games);
  };

  const onSelectPipeline = (id: string) => {
    setPipelineId(id);
    const p = pipelines.find((x) => x.id === id);
    const first = p?.files.includes("mod.lua") ? "mod.lua" : p?.files[0] ?? "";
    setOpenFiles(first ? [first] : []);
    setActiveFile(first);
    setStartStep("project");
    setView("editor");
    if (p) void autoSelectGameForPipeline(p, games);
  };

  const promptNewProject = () => {
    setPrompt({
      kind: "new-project",
      title: "New project",
      label: "Project name",
      value: projectName || "my-mod",
      confirmLabel: "Create",
    });
  };

  const newProject = async () => {
    setBusy(true);
    try {
      const folder = projectFolder.trim();
      const created = await createPipeline(
        folder
          ? { path: folder, create: true, name: projectName.trim() || undefined }
          : { name: projectName.trim() || undefined },
      );
      setOutput(`Created project ${created.id} at ${created.path}`);
      setProjectName("");
      await applyPipeline(created, { onboard: true });
    } catch (err) {
      setStatus("Create failed");
      setStatusError(true);
      setOutput(String(err));
    } finally {
      setBusy(false);
    }
  };

  const openProjectFolder = async (forcedPath?: string) => {
    const folderPath = (forcedPath ?? projectFolder).trim();
    if (!folderPath) {
      setPrompt({
        kind: "open-folder",
        title: "Open project folder",
        label: "Folder path (must contain mod.lua)",
        value: projectFolder,
        confirmLabel: "Open",
      });
      return;
    }
    setProjectFolder(folderPath);
    setBusy(true);
    try {
      const opened = await createPipeline({ path: folderPath, create: false });
      setOutput(`Opened project ${opened.id}`);
      await applyPipeline(opened, { onboard: true });
    } catch (err) {
      setStatus("Open folder failed");
      setStatusError(true);
      setOutput(String(err));
    } finally {
      setBusy(false);
    }
  };

  const newFile = (baseDir?: string) => {
    if (!pipelineId) return;
    setPrompt({
      kind: "new-file",
      title: "New file",
      label: "File path",
      value: baseDir ? `${baseDir.replace(/\/$/, "")}/new.lua` : "scripts/new.lua",
      confirmLabel: "Create",
    });
  };

  const newFolder = (baseDir?: string) => {
    if (!pipelineId) return;
    setPrompt({
      kind: "new-folder",
      title: "New folder",
      label: "Folder path",
      value: baseDir ? `${baseDir.replace(/\/$/, "")}/new-folder` : "scripts",
      confirmLabel: "Create",
    });
  };

  const runCreateFile = async (path: string) => {
    if (!pipelineId || !path) return;
    setBusy(true);
    try {
      const { pipeline, path: created } = await createPipelineEntry(pipelineId, path, "file");
      setPipelines((prev) => prev.map((p) => (p.id === pipeline.id ? pipeline : p)));
      openFileTab(created);
      setOutput(`Created ${created}`);
    } catch (err) {
      setStatus("New file failed");
      setStatusError(true);
      setOutput(String(err));
    } finally {
      setBusy(false);
    }
  };

  const runCreateFolder = async (path: string) => {
    if (!pipelineId || !path) return;
    setBusy(true);
    try {
      const { pipeline } = await createPipelineEntry(pipelineId, path, "dir");
      setPipelines((prev) => prev.map((p) => (p.id === pipeline.id ? pipeline : p)));
      setOutput(`Created folder ${path}`);
    } catch (err) {
      setStatus("New folder failed");
      setStatusError(true);
      setOutput(String(err));
    } finally {
      setBusy(false);
    }
  };

  const requestDeletePath = (path: string, isDir: boolean) => {
    if (path === "mod.lua") {
      setStatus("Cannot delete mod.lua");
      setStatusError(true);
      return;
    }
    setConfirm({ kind: "file", path, isDir });
  };

  const requestDeletePipeline = (id: string, name: string) => {
    setConfirm({ kind: "project", id, name });
  };

  const runConfirmDelete = async () => {
    if (!confirm) return;
    const job = confirm;
    setConfirm(null);
    setBusy(true);
    try {
      if (job.kind === "file") {
        if (!pipelineId) return;
        const pipeline = await deletePipelineEntry(pipelineId, job.path);
        setPipelines((prev) => prev.map((p) => (p.id === pipeline.id ? pipeline : p)));
        setOpenFiles((prev) => prev.filter((f) => f !== job.path && !f.startsWith(`${job.path}/`)));
        if (activeFile === job.path || activeFile.startsWith(`${job.path}/`)) {
          setActiveFile("");
        }
        setOutput(`Deleted ${job.path}`);
        return;
      }

      const list = await removePipeline(job.id);
      setPipelines(list);
      if (pipelineId === job.id) {
        setPipelineId("");
        setOpenFiles([]);
        setActiveFile("");
        setStartStep("project");
        setView("welcome");
      }
      setOutput(`Deleted project ${job.name}`);
      setStatus("Ready");
      setStatusError(false);
    } catch (err) {
      setStatus("Delete failed");
      setStatusError(true);
      setOutput(String(err));
    } finally {
      setBusy(false);
    }
  };

  const onPromptConfirm = (value: string) => {
    if (!prompt || !value) {
      setPrompt(null);
      return;
    }
    const kind = prompt.kind;
    setPrompt(null);
    if (kind === "new-project") {
      setProjectName(value);
      void (async () => {
        setBusy(true);
        try {
          const created = await createPipeline({ name: value });
          setOutput(`Created project ${created.id} at ${created.path}`);
          setProjectName("");
          await applyPipeline(created, { onboard: true });
        } catch (err) {
          setStatus("Create failed");
          setStatusError(true);
          setOutput(String(err));
        } finally {
          setBusy(false);
        }
      })();
      return;
    }
    if (kind === "open-folder") {
      void openProjectFolder(value);
      return;
    }
    if (kind === "new-file") {
      void runCreateFile(value);
      return;
    }
    if (kind === "new-folder") {
      void runCreateFolder(value);
    }
  };

  const closeFile = (path: string) => {
    setOpenFiles((prev) => {
      const next = prev.filter((f) => f !== path);
      if (activeFile === path) setActiveFile(next[next.length - 1] ?? "");
      return next;
    });
  };

  const navigate = (next: StudioView) => {
    if (next === "editor" && !activePipeline) return;
    if (next === "build" && !activePipeline) return;
    setView(next);
  };

  const buildStateLabel =
    busy && view === "build"
      ? "Building…"
      : buildOk === true
        ? "Build ok"
        : buildOk === false
          ? "Build failed"
          : status;

  const assetPanelProps = {
    browseMode,
    onBrowseMode,
    search,
    onSearch: (v: string) => {
      setSearch(v);
      setDeepResults(null);
      setPage(0);
    },
    onSearchGo: () => void runServerSearch(false),
    pathSearchEntries,
    folders,
    paks,
    folder,
    pak,
    sidebarCount,
    inventoryNote,
    onSelectFolder: (path: string) => {
      setFolder(path);
      setPak("");
    },
    onSelectPak: (name: string) => {
      setPak(name);
      setFolder("");
    },
    tree,
    selected,
    onSelectEntry: (e: PakEntry) => void selectEntry(e),
    onInsertSnippet: (e: PakEntry) => {
      void insertSnippet(e);
    },
    safePage,
    totalPages,
    scopedCount: scopedEntries.length,
    shownCount: pageEntries.length,
    onPageChange: setPage,
  };

  const { open: openCtx } = useContextMenu();

  return (
    <div
      className="ide"
      onContextMenu={(e) => {
        openCtx(
          e,
          [
            { id: "new-project", label: "New Project…" },
            { id: "open-folder", label: "Open Folder…" },
            { id: "sep1", label: "", separator: true },
            {
              id: "edit",
              label: "Edit Mod",
              disabled: !activePipeline,
            },
            {
              id: "build",
              label: "Build",
              disabled: !activePipeline,
            },
            { id: "sep2", label: "", separator: true },
            { id: "start", label: "Start" },
          ],
          (id) => {
            if (id === "new-project") promptNewProject();
            else if (id === "open-folder") void openProjectFolder();
            else if (id === "edit") navigate("editor");
            else if (id === "build") navigate("build");
            else if (id === "start") setView("welcome");
          },
        );
      }}
    >
      <SiteNav
        view={view}
        onNavigate={navigate}
        canEditor={!!activePipeline}
        canBuild={!!activePipeline}
      />

      <div className="ide-body">
        <AppRail
          view={view}
          onNavigate={navigate}
          canEditor={!!activePipeline}
          canBuild={!!activePipeline}
        />
        {view !== "welcome" ? (
          <ProjectSidebar
            gameLabel={gameLabel}
            gamePath={gamePath}
            gameLoaded={hasAssets}
            activeProject={activePipeline}
            selectedFile={activeFile}
            onSelectFile={openFileTab}
            onChangeGame={() => {
              setStartStep("game");
              setView("welcome");
            }}
            onSwitchProject={() => {
              setStartStep("project");
              setView("welcome");
            }}
            busy={busy}
            onNewProject={promptNewProject}
            onOpenFolder={() => void openProjectFolder()}
            onNewFile={newFile}
            onNewFolder={newFolder}
            onDeletePath={requestDeletePath}
          />
        ) : null}

        <div className={`ide-workspace ${view === "welcome" ? "ide-workspace-start" : ""}`}>
          <div className="ide-center-pane">
            {view === "welcome" ? (
              <WelcomeView
                step={startStep}
                games={games}
                pipelines={pipelines}
                pathInput={pathInput}
                onPathInput={setPathInput}
                busy={busy}
                gameLoaded={hasAssets}
                activeGameId={source}
                projectName={projectName}
                onProjectName={setProjectName}
                projectFolder={projectFolder}
                onProjectFolder={setProjectFolder}
                onPickGame={pickGame}
                onScanPath={() => void scanPath()}
                onOpenPipeline={onSelectPipeline}
                onNewProject={() => void newProject()}
                onOpenFolder={() => void openProjectFolder()}
                onOpenEditor={() => setView("editor")}
                onBackToProjects={() => setStartStep("project")}
                onDeletePipeline={requestDeletePipeline}
                onRevealPipeline={(id) => {
                  const p = pipelines.find((x) => x.id === id);
                  if (!p) return;
                  void openPath(p.path).catch((err) => {
                    setStatus(String(err));
                    setStatusError(true);
                  });
                }}
                projectLabel={activePipeline?.name || activePipeline?.id}
              />
            ) : null}

            {view === "editor" ? (
              <div className="ide-center editor-center">
                <div className="editor-row">
                  <div className="ide-editor-wrap">
                    <ModEditor
                      pipelineId={pipelineId}
                      openFiles={openFiles}
                      activeFile={activeFile}
                      onSelectFile={setActiveFile}
                      onCloseFile={closeFile}
                      insertText={insertText}
                      onInsertConsumed={() => setInsertText(null)}
                    />
                  </div>
                  {hasAssets ? (
                    <AssetPreviewPane
                      selected={selected}
                      preview={preview}
                      previewError={previewError}
                      busy={busy}
                      formatBytes={formatBytes}
                      onInsertSnippet={() => void insertSnippet()}
                      canInsert={!!selected && !busy && !!pipelineId}
                    />
                  ) : null}
                </div>
                <div className="ide-bottom">
                  <div className="ide-bottom-tabs">
                    <button
                      type="button"
                      className={`ide-bottom-tab ${bottomTab === "output" ? "ide-bottom-tab-active" : ""}`}
                      onClick={() => setBottomTab("output")}
                    >
                      Output
                    </button>
                    <button
                      type="button"
                      className={`ide-bottom-tab ${bottomTab === "console" ? "ide-bottom-tab-active" : ""}`}
                      onClick={() => setBottomTab("console")}
                    >
                      Console
                    </button>
                    <button
                      type="button"
                      className="btn-primary btn-sm ide-btn-build"
                      disabled={busy || !activePipeline}
                      onClick={() => setView("build")}
                    >
                      Build…
                    </button>
                  </div>
                  <pre className={`ide-bottom-body ${statusError ? "ide-bottom-error" : ""}`}>
                    {bottomTab === "output" ? output : busy ? "Busy…" : "Ready."}
                  </pre>
                </div>
              </div>
            ) : null}

            {view === "build" ? (
              <BuildView
                pipeline={activePipeline}
                gameLabel={gameLabel}
                busy={busy}
                compress={compress}
                onCompressChange={setCompress}
                forceExtract={forceExtract}
                onForceExtractChange={setForceExtract}
                outputPath={outputPath}
                onOutputPathChange={setOutputPath}
                buildLog={buildLog}
                buildOk={buildOk}
                lastOutputPak={lastOutputPak}
                durationMs={durationMs}
                history={history}
                onBuild={() => void buildMod()}
                onDeploy={() => void deployMod()}
                onCopyPath={() => {
                  if (lastOutputPak) void navigator.clipboard.writeText(lastOutputPak);
                }}
                onOpenFolder={() => {
                  if (!lastOutputPak) return;
                  void openPath(lastOutputPak).catch((err) => {
                    setOutput(String(err));
                    setStatusError(true);
                  });
                }}
                onClearLog={() => setBuildLog("")}
              />
            ) : null}
          </div>

          {view === "editor" ? (
            <AssetSidebar hasAssets={hasAssets} busy={busy} {...assetPanelProps} />
          ) : null}
        </div>
      </div>

      <footer className="ide-status">
        <span className={statusError ? "status-error" : ""}>{buildStateLabel}</span>
        <span className="ide-status-sep" />
        <span>{gameLabel || "No game"}</span>
        <span className="ide-status-sep" />
        <span>Mod: {activePipeline?.id ?? "none"}</span>
        <span className="ide-status-sep" />
        <span>{hasAssets ? `${catalogEntries.length} ${browseMode === "json" ? "json" : "files"}` : "no game"}</span>
      </footer>

      <PromptDialog
        open={!!prompt}
        title={prompt?.title ?? ""}
        label={prompt?.label}
        defaultValue={prompt?.value ?? ""}
        confirmLabel={prompt?.confirmLabel}
        onCancel={() => setPrompt(null)}
        onConfirm={onPromptConfirm}
      />
      <ConfirmDialog
        open={!!confirm}
        title={
          confirm?.kind === "project"
            ? "Delete project"
            : confirm?.isDir
              ? "Delete folder"
              : "Delete file"
        }
        message={
          confirm?.kind === "project"
            ? `Delete project “${confirm.name}” and its files? This cannot be undone.`
            : confirm
              ? `Delete ${confirm.isDir ? "folder" : "file"} “${confirm.path}”? This cannot be undone.`
              : ""
        }
        confirmLabel="Delete"
        onCancel={() => setConfirm(null)}
        onConfirm={() => void runConfirmDelete()}
      />
    </div>
  );
}
