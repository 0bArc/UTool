import type { NextApiRequest, NextApiResponse } from "next";
import { existsSync, statSync } from "node:fs";
import { dirname, isAbsolute, normalize, resolve } from "node:path";
import { spawn } from "node:child_process";
import { platform } from "node:os";
import { resolveRepoRoot } from "@/lib/utool-config";

function resolveRevealTarget(raw: string): { path: string; select: boolean } {
  const abs = normalize(isAbsolute(raw) ? raw : resolve(resolveRepoRoot(), raw));

  if (existsSync(abs)) {
    return { path: abs, select: statSync(abs).isFile() };
  }

  // Common after pak.zip: the .pak is deleted but the zip (or folder) remains.
  const zipSibling = abs.replace(/\.pak$/i, ".zip");
  if (existsSync(zipSibling)) {
    return { path: zipSibling, select: true };
  }

  const parent = dirname(abs);
  if (existsSync(parent)) {
    return { path: parent, select: false };
  }

  throw new Error("Path not found");
}

function revealInExplorer(target: string, select: boolean): void {
  const normalized = normalize(target);

  if (platform() === "win32") {
    const args = select ? [`/select,${normalized}`] : [normalized];
    spawn("explorer.exe", args, { detached: true, stdio: "ignore", windowsHide: true }).unref();
    return;
  }

  if (platform() === "darwin") {
    spawn("open", select ? ["-R", normalized] : [normalized], { detached: true, stdio: "ignore" }).unref();
    return;
  }

  const dir = select ? dirname(normalized) : normalized;
  spawn("xdg-open", [dir], { detached: true, stdio: "ignore" }).unref();
}

export default function handler(req: NextApiRequest, res: NextApiResponse) {
  if (req.method !== "POST") {
    res.setHeader("Allow", "POST");
    res.status(405).json({ ok: false, error: "Method not allowed" });
    return;
  }

  const body = typeof req.body === "string" ? JSON.parse(req.body || "{}") : (req.body ?? {});
  const raw = typeof body.path === "string" ? body.path.trim() : "";
  if (!raw) {
    res.status(400).json({ ok: false, error: "path required" });
    return;
  }

  try {
    const { path, select } = resolveRevealTarget(raw);
    revealInExplorer(path, select);
    res.status(200).json({ ok: true, opened: select ? dirname(path) : path, selected: select ? path : undefined });
  } catch (err) {
    const message = err instanceof Error ? err.message : String(err);
    res.status(404).json({ ok: false, error: message });
  }
}
