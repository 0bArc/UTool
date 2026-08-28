import { rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { parsePort, spawnNext } from "./cli-utils.mjs";
import { projectCwd, repoRoot } from "./with-subst.mjs";

const argv = process.argv.slice(2);
const cwd = projectCwd();
const port = parsePort(argv);

writeFileSync(join(cwd, ".repo-root"), repoRoot, "utf8");

try {
  rmSync(join(cwd, ".next"), { recursive: true, force: true });
} catch {
  /* ignore */
}

console.log(`Pak Studio dev → http://127.0.0.1:${port}/`);
console.log(`cwd: ${cwd}`);

const child = spawnNext(cwd, ["dev", "--port", port]);

child.on("exit", (code) => process.exit(code ?? 0));
