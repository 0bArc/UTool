import type { NextApiRequest, NextApiResponse } from "next";
import {
  createPipelineFromExample,
  listPipelines,
  openOrCreatePipelineAt,
  removePipeline,
} from "@/lib/pipelines";

export default function handler(req: NextApiRequest, res: NextApiResponse) {
  if (req.method === "GET") {
    try {
      res.status(200).json({ ok: true, pipelines: listPipelines() });
    } catch (err) {
      res.status(500).json({ ok: false, error: err instanceof Error ? err.message : String(err) });
    }
    return;
  }

  if (req.method === "POST") {
    try {
      const body = typeof req.body === "string" ? JSON.parse(req.body || "{}") : (req.body ?? {});
      const name = typeof body.name === "string" ? body.name.trim() : "";
      const id = typeof body.id === "string" ? body.id.trim() : "";
      const folder = typeof body.path === "string" ? body.path.trim() : "";
      const create = body.create === true;

      const pipeline = folder
        ? openOrCreatePipelineAt(folder, { create, name: name || undefined })
        : createPipelineFromExample(id || undefined, name || undefined);

      res.status(201).json({ ok: true, pipeline });
    } catch (err) {
      res.status(500).json({ ok: false, error: err instanceof Error ? err.message : String(err) });
    }
    return;
  }

  if (req.method === "DELETE") {
    try {
      const body = typeof req.body === "string" ? JSON.parse(req.body || "{}") : (req.body ?? {});
      const id =
        typeof body.id === "string"
          ? body.id.trim()
          : typeof req.query.id === "string"
            ? req.query.id.trim()
            : "";
      if (!id) {
        res.status(400).json({ ok: false, error: "id required" });
        return;
      }
      removePipeline(id);
      res.status(200).json({ ok: true, pipelines: listPipelines() });
    } catch (err) {
      res.status(500).json({ ok: false, error: err instanceof Error ? err.message : String(err) });
    }
    return;
  }

  res.setHeader("Allow", "GET, POST, DELETE");
  res.status(405).json({ ok: false, error: "Method not allowed" });
}
