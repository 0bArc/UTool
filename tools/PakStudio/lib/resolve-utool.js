const { spawnSync } = require("node:child_process");
const { existsSync } = require("node:fs");
const { dirname, join } = require("node:path");

function supportsPakList(exe) {
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

/**
 * @param {{ preferred?: string, repoRoot: string }} opts
 */
function resolveUtoolExe(opts) {
  const { preferred, repoRoot } = opts;
  const personalBuild = join(dirname(dirname(repoRoot)), "utool-build", "utool.exe");
  const candidates = [
    preferred,
    join(repoRoot, "dist", "utool", "utool.exe"),
    personalBuild,
    process.env.LOCALAPPDATA ? join(process.env.LOCALAPPDATA, "utool", "utool.exe") : null,
  ].filter(Boolean);

  for (const exe of candidates) {
    if (!existsSync(exe)) continue;
    if (supportsPakList(exe)) return exe;
  }

  for (const exe of candidates) {
    if (exe && existsSync(exe)) return exe;
  }

  return preferred || "utool";
}

function staleUtoolMessage(exe) {
  return (
    `utool at ${exe} is outdated (no pak list). ` +
    "Rebuild csStratware (cmake/build-release.cmd) and set UTOOL_EXE, " +
    "or delete %LOCALAPPDATA%\\utool\\utool.exe."
  );
}

module.exports = { resolveUtoolExe, supportsPakList, staleUtoolMessage };
