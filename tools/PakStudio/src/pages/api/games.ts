import type { NextApiRequest, NextApiResponse } from "next";
import { listGamesFromConfig, probeInstallPath, utoolCwd } from "@/lib/utool-config";

export default async function handler(req: NextApiRequest, res: NextApiResponse) {
  const cwd = utoolCwd();

  if (req.method === "GET") {
    res.status(200).json({ ok: true, ...listGamesFromConfig(), cwd });
    return;
  }

  if (req.method === "POST") {
    const { path } = req.body as { path?: string };
    if (!path || typeof path !== "string") {
      res.status(400).json({ ok: false, error: "path required" });
      return;
    }

    const probe = probeInstallPath(path);
    if (!probe.ready) {
      res.status(200).json({
        ok: false,
        error: "No paks found. Paste the game install folder or Content/Paks path.",
        ...probe,
        cwd,
      });
      return;
    }

    res.status(200).json({ ok: true, ...probe, cwd });
    return;
  }

  res.setHeader("Allow", "GET, POST");
  res.status(405).json({ ok: false, error: "Method not allowed" });
}
