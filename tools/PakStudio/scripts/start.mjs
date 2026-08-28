import { writeFileSync } from "node:fs";
import { join } from "node:path";
import { parsePort, spawnNext } from "./cli-utils.mjs";
import { projectCwd, repoRoot } from "./with-subst.mjs";

const port = parsePort(process.argv.slice(2));
const cwd = projectCwd();
writeFileSync(join(cwd, ".repo-root"), repoRoot, "utf8");
const child = spawnNext(cwd, ["start", "--port", port]);

child.on("exit", (code) => process.exit(code ?? 0));
