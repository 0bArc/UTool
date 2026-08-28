import type { NextApiRequest, NextApiResponse } from "next";
import { utoolCwd } from "@/lib/utool-config";
import { runUtool } from "@/lib/utool-server";

export default async function handler(req: NextApiRequest, res: NextApiResponse) {
  if (req.method !== "POST") {
    res.setHeader("Allow", "POST");
    res.status(405).json({ ok: false, error: "Method not allowed" });
    return;
  }

  try {
    const { args } = req.body as { args?: unknown };
    if (!Array.isArray(args)) {
      res.status(400).json({ ok: false, error: "args must be an array" });
      return;
    }
    const data = await runUtool(args.map(String));
    res.status(200).json({ ok: true, data, cwd: utoolCwd() });
  } catch (err) {
    const message = err instanceof Error ? err.message : String(err);
    res.status(500).json({ ok: false, error: message, cwd: utoolCwd() });
  }
}
