import { writeFileSync } from "node:fs";
import { join } from "node:path";
import { spawnNextSync } from "./cli-utils.mjs";
import { projectCwd, repoRoot } from "./with-subst.mjs";

const cwd = projectCwd();
writeFileSync(join(cwd, ".repo-root"), repoRoot, "utf8");console.log(`Building Pak Studio (cwd: ${cwd})`);

const result = spawnNextSync(cwd, ["build"]);

process.exit(result.status ?? 1);
