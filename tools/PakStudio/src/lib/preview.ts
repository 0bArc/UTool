export function formatPreviewResponse(doc: unknown): string {
  if (!doc || typeof doc !== "object") return String(doc);
  const { kind = "unknown", payload } = doc as {
    kind?: string;
    payload?: Record<string, unknown>;
  };
  const lines: string[] = [`[${kind}]`];

  if (payload && typeof payload === "object") {
    if (typeof payload.extractedPath === "string")
      lines.push(`extracted: ${payload.extractedPath}`);
    if (payload.cached) lines.push("cached: true");
    if (Array.isArray(payload.paths) && payload.paths.length > 0) {
      lines.push("", "paths:");
      for (const p of payload.paths.slice(0, 40)) lines.push(`  ${String(p)}`);
    }
    if (typeof payload.pretty === "string") {
      lines.push("", payload.pretty);
      return lines.join("\n");
    }
    if (typeof payload.text === "string") {
      lines.push("", payload.text);
      return lines.join("\n");
    }
    if (Array.isArray(payload.keys)) {
      lines.push("", "curve keys:");
      for (const k of payload.keys.slice(0, 40) as Array<{ time?: number; value?: number }>) {
        lines.push(`  t=${k.time} v=${k.value}`);
      }
      return lines.join("\n");
    }
    if (Array.isArray(payload.strings) && payload.strings.length > 0) {
      lines.push("", "strings:");
      for (const s of payload.strings.slice(0, 60)) lines.push(`  ${String(s)}`);
    }
    if (typeof payload.hexHead === "string") lines.push("", `hex: ${payload.hexHead}`);
    if (typeof payload.note === "string") lines.push(payload.note);
    if (lines.length <= 1) lines.push("", JSON.stringify(payload, null, 2));
  }

  return lines.join("\n");
}

export function formatSnippetResponse(doc: unknown): string {
  if (!doc || typeof doc !== "object") return "";
  return (doc as { snippet?: string }).snippet ?? "";
}

/** Strip utool preview metadata; return Monaco-friendly body + language. */
export function extractPreviewBody(
  text: string,
  extension?: string,
): { language: string; content: string } {
  if (!text.trim()) return { language: "plaintext", content: "" };

  const lines = text.split("\n");
  let start = 0;
  for (let i = 0; i < lines.length; i++) {
    const trimmed = lines[i].trim();
    if (trimmed.startsWith("{") || trimmed.startsWith("[")) {
      start = i;
      break;
    }
  }

  const body = lines.slice(start).join("\n").trim();
  if (!body) return { language: "plaintext", content: text.trim() };

  const looksJson =
    extension === "json" || body.startsWith("{") || body.startsWith("[");
  if (!looksJson) return { language: "plaintext", content: body };

  try {
    return {
      language: "json",
      content: JSON.stringify(JSON.parse(body), null, 2),
    };
  } catch {
    return { language: "json", content: body };
  }
}
