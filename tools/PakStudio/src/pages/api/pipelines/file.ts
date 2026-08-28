import type { NextApiRequest, NextApiResponse } from "next";
import {
  createPipelineEntry,
  deletePipelineEntry,
  readPipelineFile,
  writePipelineFile,
} from "@/lib/pipelines";

export default function handler(req: NextApiRequest, res: NextApiResponse) {
  try {
    if (req.method === "GET") {
      const id = String(req.query.id ?? "");
      const path = String(req.query.path ?? "");
      if (!id || !path) {
        res.status(400).json({ ok: false, error: "id and path required" });
        return;
      }
      const content = readPipelineFile(id, path);
      res.status(200).json({ ok: true, id, path, content });
      return;
    }

    if (req.method === "PUT") {
      const body = req.body as { id?: string; path?: string; content?: string };
      if (!body.id || !body.path || typeof body.content !== "string") {
        res.status(400).json({ ok: false, error: "id, path, content required" });
        return;
      }
      writePipelineFile(body.id, body.path, body.content);
      res.status(200).json({ ok: true, id: body.id, path: body.path });
      return;
    }

    if (req.method === "POST") {
      const body = typeof req.body === "string" ? JSON.parse(req.body || "{}") : (req.body ?? {});
      const id = typeof body.id === "string" ? body.id : "";
      const path = typeof body.path === "string" ? body.path.trim() : "";
      const kind = body.kind === "dir" ? "dir" : "file";
      const content = typeof body.content === "string" ? body.content : undefined;
      if (!id || !path) {
        res.status(400).json({ ok: false, error: "id and path required" });
        return;
      }
      const pipeline = createPipelineEntry(id, path, kind, content);
      res.status(201).json({ ok: true, pipeline, path });
      return;
    }

    if (req.method === "DELETE") {
      const body = typeof req.body === "string" ? JSON.parse(req.body || "{}") : (req.body ?? {});
      const id =
        typeof body.id === "string"
          ? body.id
          : typeof req.query.id === "string"
            ? req.query.id
            : "";
      const path =
        typeof body.path === "string"
          ? body.path.trim()
          : typeof req.query.path === "string"
            ? req.query.path.trim()
            : "";
      if (!id || !path) {
        res.status(400).json({ ok: false, error: "id and path required" });
        return;
      }
      const pipeline = deletePipelineEntry(id, path);
      res.status(200).json({ ok: true, pipeline });
      return;
    }

    res.setHeader("Allow", "GET, PUT, POST, DELETE");
    res.status(405).json({ ok: false, error: "Method not allowed" });
  } catch (err) {
    res.status(500).json({ ok: false, error: err instanceof Error ? err.message : String(err) });
  }
}
