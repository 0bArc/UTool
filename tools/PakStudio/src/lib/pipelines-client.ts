export type PipelineInfo = {
  id: string;
  path: string;
  name: string;
  files: string[];
  source: "manifest" | "discover" | "user";
};

export async function fetchPipelines(): Promise<PipelineInfo[]> {
  const res = await fetch("/api/pipelines");
  const doc = (await res.json()) as { ok?: boolean; pipelines?: PipelineInfo[]; error?: string };
  if (!res.ok || !doc.ok) throw new Error(doc.error ?? "Failed to list pipelines");
  return doc.pipelines ?? [];
}

export async function createPipeline(opts?: {
  id?: string;
  name?: string;
  path?: string;
  create?: boolean;
}): Promise<PipelineInfo> {
  const res = await fetch("/api/pipelines", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(opts ?? {}),
  });
  const doc = (await res.json()) as { ok?: boolean; pipeline?: PipelineInfo; error?: string };
  if (!res.ok || !doc.ok || !doc.pipeline) throw new Error(doc.error ?? "Failed to create pipeline");
  return doc.pipeline;
}

export async function fetchPipelineFile(id: string, path: string): Promise<string> {
  const res = await fetch(
    `/api/pipelines/file?id=${encodeURIComponent(id)}&path=${encodeURIComponent(path)}`,
  );
  const doc = (await res.json()) as { ok?: boolean; content?: string; error?: string };
  if (!res.ok || !doc.ok) throw new Error(doc.error ?? "Failed to read file");
  return doc.content ?? "";
}

export async function savePipelineFile(id: string, path: string, content: string): Promise<void> {
  const res = await fetch("/api/pipelines/file", {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ id, path, content }),
  });
  const doc = (await res.json()) as { ok?: boolean; error?: string };
  if (!res.ok || !doc.ok) throw new Error(doc.error ?? "Failed to save file");
}

export async function createPipelineEntry(
  id: string,
  path: string,
  kind: "file" | "dir",
  content?: string,
): Promise<{ pipeline: PipelineInfo; path: string }> {
  const res = await fetch("/api/pipelines/file", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ id, path, kind, content }),
  });
  const doc = (await res.json()) as {
    ok?: boolean;
    pipeline?: PipelineInfo;
    path?: string;
    error?: string;
  };
  if (!res.ok || !doc.ok || !doc.pipeline) throw new Error(doc.error ?? "Failed to create entry");
  return { pipeline: doc.pipeline, path: doc.path ?? path };
}

export async function deletePipelineEntry(
  id: string,
  path: string,
): Promise<PipelineInfo> {
  const res = await fetch("/api/pipelines/file", {
    method: "DELETE",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ id, path }),
  });
  const doc = (await res.json()) as {
    ok?: boolean;
    pipeline?: PipelineInfo;
    error?: string;
  };
  if (!res.ok || !doc.ok || !doc.pipeline) throw new Error(doc.error ?? "Failed to delete entry");
  return doc.pipeline;
}

export async function removePipeline(id: string): Promise<PipelineInfo[]> {
  const res = await fetch("/api/pipelines", {
    method: "DELETE",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ id }),
  });
  const doc = (await res.json()) as {
    ok?: boolean;
    pipelines?: PipelineInfo[];
    error?: string;
  };
  if (!res.ok || !doc.ok) throw new Error(doc.error ?? "Failed to delete project");
  return doc.pipelines ?? [];
}
