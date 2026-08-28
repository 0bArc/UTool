export async function utool(args: string[]): Promise<string> {
  const res = await fetch("/api/utool", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ args }),
  });
  const doc = (await res.json()) as { ok: boolean; data?: string; error?: string };
  if (!doc.ok) throw new Error(doc.error || "utool failed");
  return doc.data ?? "";
}
