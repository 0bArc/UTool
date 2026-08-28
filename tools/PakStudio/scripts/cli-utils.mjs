import { spawn, spawnSync } from "node:child_process";
import { existsSync } from "node:fs";
import { join } from "node:path";
import { resolveUtoolExe } from "../lib/resolve-utool.js";
import { projectCwd, repoRoot } from "./with-subst.mjs";

/** Env for Next.js so utool.json resolves from the repo, not SUBST cwd. */
export function studioEnv() {
  const env = { ...process.env };
  env.PAK_STUDIO_ROOT = projectCwd();
  env.UTOOL_REPO_ROOT = repoRoot;

  if (!env.UTOOL_CONFIG_DIR) {
    const examples = join(repoRoot, "examples");
    if (existsSync(join(examples, "utool.json"))) {
      env.UTOOL_CONFIG_DIR = examples;
    } else if (existsSync(join(repoRoot, "utool.json"))) {
      env.UTOOL_CONFIG_DIR = repoRoot;
    }
  }

  env.UTOOL_EXE = resolveUtoolExe({ preferred: env.UTOOL_EXE, repoRoot });
  return env;
}

/** @param {string[]} argv process.argv.slice(2) */
export function parsePort(argv) {
  if (process.env.PAK_STUDIO_PORT) return process.env.PAK_STUDIO_PORT;

  for (let i = 0; i < argv.length; i++) {
    const arg = argv[i];
    if (arg === "--port" || arg === "-p") {
      const next = argv[i + 1];
      if (next && !next.startsWith("-")) return next;
    }
    if (arg.startsWith("--port=")) return arg.slice("--port=".length);
  }

  return "3000";
}

function nextBin(cwd) {
  return join(cwd, "node_modules", "next", "dist", "bin", "next");
}

export function spawnNext(cwd, args, stdio = "inherit") {
  return spawn(process.execPath, [nextBin(cwd), ...args], {
    cwd,
    stdio,
    shell: false,
    env: studioEnv(),
  });
}

export function spawnNextSync(cwd, args, stdio = "inherit") {
  return spawnSync(process.execPath, [nextBin(cwd), ...args], {
    cwd,
    stdio,
    shell: false,
    env: studioEnv(),
  });
}
