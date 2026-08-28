import { spawn } from "node:child_process";
import { parsePort } from "./cli-utils.mjs";
import { projectCwd } from "./with-subst.mjs";

const argv = process.argv.slice(2);
const cwd = projectCwd();
const port = parsePort(argv);
const url = `http://127.0.0.1:${port}/`;

const child = spawn(process.execPath, ["scripts/dev.mjs", ...argv], {
  cwd,
  detached: true,
  stdio: "ignore",
  shell: false,
  env: { ...process.env, PAK_STUDIO_PORT: port },
});
child.unref();

setTimeout(() => {
  spawn("cmd", ["/c", "start", "", url], { detached: true, stdio: "ignore" }).unref();
  console.log(`Opened ${url}`);
}, 2000);
