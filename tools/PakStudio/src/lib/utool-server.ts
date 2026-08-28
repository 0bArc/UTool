import { spawn, spawnSync } from "node:child_process";
import { existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { utoolCwd } from "@/lib/utool-config";

function repoRootFromConfig(): string {
  const cwd = utoolCwd();
  if (existsSync(join(cwd, "utool.json"))) {
    const parent = dirname(cwd);
    if (existsSync(join(parent, "examples", "utool.json")) || existsSync(join(parent, "CMakeLists.txt"))) {
      return parent;
    }
    return cwd;
  }
  return join(cwd, "..", "..");
}

function supportsPakList(exe: string): boolean {
  try {
    const result = spawnSync(exe, ["help"], {
      encoding: "utf8",
      windowsHide: true,
      timeout: 15000,
    });
    const text = `${result.stdout || ""}${result.stderr || ""}`;
    return text.includes("pak list");
  } catch {
    return false;
  }
}

export function findUtoolExe(): string {
  if (process.env.UTOOL_EXE && existsSync(process.env.UTOOL_EXE) && supportsPakList(process.env.UTOOL_EXE)) {
    return process.env.UTOOL_EXE;
  }

  const repo =
    process.env.UTOOL_REPO_ROOT && existsSync(process.env.UTOOL_REPO_ROOT)
      ? process.env.UTOOL_REPO_ROOT
      : repoRootFromConfig();

  const personalBuild = join(dirname(dirname(repo)), "utool-build", "utool.exe");
  const candidates = [
    process.env.UTOOL_EXE,
    join(repo, "dist", "utool", "utool.exe"),
    personalBuild,
    process.env.LOCALAPPDATA ? join(process.env.LOCALAPPDATA, "utool", "utool.exe") : null,
  ].filter((c): c is string => Boolean(c));

  for (const exe of candidates) {
    if (!existsSync(exe)) continue;
    if (supportsPakList(exe)) return exe;
  }

  for (const exe of candidates) {
    if (existsSync(exe)) return exe;
  }

  return "utool";
}

function formatUtoolError(exe: string, stderr: string, stdout: string): string {
  const text = stderr || stdout;
  if (text.includes("expected 'build-mod'") || text.includes("unknown subcommand")) {
    return (
      `Outdated utool at ${exe} (missing pak list/search/preview). ` +
      "Rebuild with .\\cmake\\build-release.cmd and restart Pak Studio with UTOOL_EXE set, " +
      "or remove the old copy in %LOCALAPPDATA%\\utool\\."
    );
  }
  return text || `utool exited with an error`;
}

export function runUtool(args: string[]): Promise<string> {
  return new Promise((resolve, reject) => {
    const exe = findUtoolExe();
    const cwd = utoolCwd();
    const proc = spawn(exe, args, { windowsHide: true, cwd });
    let stdout = "";
    let stderr = "";
    proc.stdout.on("data", (chunk: Buffer) => {
      stdout += chunk;
    });
    proc.stderr.on("data", (chunk: Buffer) => {
      stderr += chunk;
    });
    proc.on("error", reject);
    proc.on("close", (code) => {
      if (code === 0) resolve(stdout);
      else reject(new Error(formatUtoolError(exe, stderr, stdout)));
    });
  });
}
