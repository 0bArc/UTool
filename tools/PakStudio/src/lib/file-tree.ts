export type FileTreeNode = {
  name: string;
  path: string;
  kind: "file" | "dir";
  children?: FileTreeNode[];
};

export function buildProjectFileTree(files: string[]): FileTreeNode[] {
  type Mutable = {
    name: string;
    path: string;
    kind: "file" | "dir";
    children?: Map<string, Mutable>;
  };
  const root = new Map<string, Mutable>();

  const ensureDir = (
    map: Map<string, Mutable>,
    parts: string[],
    depth: number,
  ): Map<string, Mutable> => {
    if (depth >= parts.length - 1) return map;
    const name = parts[depth];
    const path = parts.slice(0, depth + 1).join("/");
    let node = map.get(name);
    if (!node || node.kind !== "dir") {
      node = { name, path, kind: "dir", children: new Map() };
      map.set(name, node);
    }
    if (!node.children) node.children = new Map();
    return ensureDir(node.children, parts, depth + 1);
  };

  for (const file of files) {
    const isEmptyDir = file.endsWith("/");
    const parts = (isEmptyDir ? file.slice(0, -1) : file).split("/").filter(Boolean);
    if (parts.length === 0) continue;
    if (isEmptyDir) {
      let map = root;
      for (let depth = 0; depth < parts.length; depth++) {
        const name = parts[depth];
        const path = parts.slice(0, depth + 1).join("/");
        let node = map.get(name);
        if (!node || node.kind !== "dir") {
          node = { name, path, kind: "dir", children: new Map() };
          map.set(name, node);
        }
        if (!node.children) node.children = new Map();
        map = node.children;
      }
      continue;
    }
    const parent = ensureDir(root, parts, 0);
    const name = parts[parts.length - 1];
    parent.set(name, { name, path: file, kind: "file" });
  }

  const toList = (map: Map<string, Mutable>): FileTreeNode[] =>
    [...map.values()]
      .sort((a, b) => {
        if (a.kind !== b.kind) return a.kind === "dir" ? -1 : 1;
        return a.name.localeCompare(b.name);
      })
      .map((n) => ({
        name: n.name,
        path: n.path,
        kind: n.kind,
        children: n.children ? toList(n.children) : undefined,
      }));

  return toList(root);
}
