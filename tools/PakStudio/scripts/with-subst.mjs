import { spawnSync } from "node:child_process";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const pakStudioRoot = join(dirname(fileURLToPath(import.meta.url)), "..");
const drive = process.env.PAK_STUDIO_DRIVE || "P:";

/** csStratware repo root (real path, not SUBST). */
export const repoRoot = join(pakStudioRoot, "..", "..");

/** Next.js breaks on '#' in paths — run from SUBST drive on Windows. */
export function projectCwd() {
  if (process.platform !== "win32" || !pakStudioRoot.includes("#")) {
    return pakStudioRoot;
  }

  spawnSync("subst", [drive, "/d"], { stdio: "ignore", windowsHide: true });
  spawnSync("subst", [drive, pakStudioRoot], { stdio: "ignore", windowsHide: true });
  return `${drive}\\`;
}
