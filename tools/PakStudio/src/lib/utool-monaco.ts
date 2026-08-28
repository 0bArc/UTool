import type { Monaco } from "@monaco-editor/react";

let registered = false;

export function registerUtoolLua(monaco: Monaco): void {
  if (registered) return;
  registered = true;

  const kind = monaco.languages.CompletionItemKind;
  const snippet = monaco.languages.CompletionItemInsertTextRule.InsertAsSnippet;

  const utoolRoot = [
    {
      label: "mod",
      detail: "Register mod manifest (once per mod.lua)",
      insertText:
        'mod {\n  id = "${1:mod.id}",\n  name = "${2:Mod Name}",\n  version = "1.0.0",\n  target = { gameId = "${3:Icarus}" },\n  pak = {\n    output = "dist/${4:mod}_P.pak",\n    mountPoint = "@auto",\n    sourcePak = "@data",\n  },\n}\n',
    },
    {
      label: "asset",
      detail: "Load JSON table from @data",
      insertText: 'asset("${1:D_Example.json}")',
    },
    {
      label: "patch_curve",
      detail: "Patch a curve asset",
      insertText:
        'patch_curve("${1:C_PlayerExperienceGrowth}", "${2:Data/Character}", function()\n  local last = utool.curve:LastKey()\n  utool.curve:AddKey(last.Time + 1, last.Value + ${3:100})\nend)',
    },
    {
      label: "patch_asset",
      detail: "Low-level JSON asset editor callback",
      insertText:
        'patch_asset("${1:D_Example.json}", function()\n  ${2:-- utool.editor ops}\nend)',
    },
    { label: "editor", detail: "JsonAssetEditor (inside patch_asset)", insertText: "editor" },
    { label: "curve", detail: "CurveEditor (inside patch_curve)", insertText: "curve" },
    { label: "pak", detail: "Multi-pak variant builder", insertText: "pak" },
  ];

  const assetChain = [
    { label: "row", detail: "Row where Name matches", insertText: 'row("${1:RowName}")' },
    {
      label: "find",
      detail: "Find row in collection",
      insertText: 'find("${1:Rows}", { ${2:Name} = "${3:value}" })',
    },
    {
      label: "map",
      detail: "Map each element in collection",
      insertText:
        'map("${1:Rows}", function(row)\n  ${2:-- mutate row}\n  return row\nend)',
    },
  ];

  const rowChain = [
    { label: "field", detail: "Field on current row", insertText: 'field("${1:Property}")' },
    { label: "set", detail: "Set value on row or field", insertText: "set(${1:value})" },
  ];

  const pakChain = [
    {
      label: "create",
      detail: "Spawn a variant pak from a field ref",
      insertText: "create(${1:fieldRef}):Value(${2:0})",
    },
  ];

  const curveChain = [
    { label: "AssetName", detail: "Current curve asset name", insertText: "AssetName" },
    { label: "LastKey", detail: "Last key on curve", insertText: "LastKey()" },
    { label: "AddKey", detail: "Append curve key", insertText: "AddKey(${1:time}, ${2:value})" },
    { label: "SetKey", detail: "Replace curve key", insertText: "SetKey(${1:time}, ${2:value})" },
  ];

  const modFields = [
    "id",
    "name",
    "version",
    "description",
    "author",
    "updateVersion",
    "target",
    "scripts",
    "contentRoots",
    "pak",
    "gameId",
    "output",
    "mountPoint",
    "sourcePak",
    "curveSourcePak",
  ];

  monaco.languages.registerCompletionItemProvider("lua", {
    triggerCharacters: [".", ":", '"', "{"],
    provideCompletionItems(model: { getWordUntilPosition: (p: { lineNumber: number; column: number }) => { startColumn: number; endColumn: number; word: string }; getLineContent: (n: number) => string }, position: { lineNumber: number; column: number }) {
      const word = model.getWordUntilPosition(position);
      const range = {
        startLineNumber: position.lineNumber,
        endLineNumber: position.lineNumber,
        startColumn: word.startColumn,
        endColumn: word.endColumn,
      };
      const line = model.getLineContent(position.lineNumber);
      const before = line.slice(0, position.column - 1);
      const suggestions: Array<{
        label: string;
        kind: number;
        detail?: string;
        insertText: string;
        insertTextRules?: number;
        range: typeof range;
      }> = [];

      const push = (
        items: Array<{ label: string; insertText: string; detail: string }>,
        prefix = "",
      ) => {
        for (const item of items) {
          suggestions.push({
            label: item.label,
            kind: kind.Method,
            detail: item.detail,
            insertText: prefix + item.insertText,
            insertTextRules: snippet,
            range,
          });
        }
      };

      if (/\butool\.[\w]*$/.test(before)) push(utoolRoot);
      else if (/\butool\.pak\.[\w]*$/.test(before)) push(pakChain);
      else if (/\butool\.curve:[\w]*$/.test(before)) push(curveChain);
      else if (/utool\.asset\([^)]*\):[\w]*$/.test(before)) push(assetChain);
      else if (/:\w+\([^)]*\):[\w]*$/.test(before) || /:\):[\w]*$/.test(before)) push(rowChain);
      else if (/\butool\.mod\s*\{[\s\S]*$/.test(before) || /^\s*[\w]*$/.test(before)) {
        for (const field of modFields) {
          if (!field.startsWith(word.word)) continue;
          suggestions.push({
            label: field,
            kind: kind.Property,
            detail: "mod manifest field",
            insertText: `${field} = ${field === "scripts" ? '{ "${1:scripts/extra.lua}" }' : field === "target" ? '{ gameId = "${1:Icarus}" }' : field === "pak" ? '{\n  output = "${1:dist/mod_P.pak}",\n  mountPoint = "@auto",\n  sourcePak = "@data",\n}' : '"${1}"'}`,
            insertTextRules: snippet,
            range,
          });
        }
      }

      if (/^u[\w]*$/.test(word.word) && word.word.length <= 5) {
        suggestions.push({
          label: "utool",
          kind: kind.Module,
          detail: "UTool mod API",
          insertText: "utool",
          range,
        });
      }

      return { suggestions };
    },
  });

  const hoverDocs: Record<string, string> = {
    mod: "Register mod manifest. Required: `id`, `name`. Call once.",
    asset: "Load a JSON table. Chain `:row`, `:find`, `:map`, `:field`, `:set`.",
    patch_curve: "Edit curve keys inside callback. Use `utool.curve` helpers.",
    patch_asset: "Edit raw JSON via `utool.editor` inside callback.",
    pak: "Build multi-variant paks with `utool.pak.create(field):Value(n)`.",
    row: "Select `/Rows` entry where `Name == name`.",
    find: "Match rows in a collection by property values.",
    map: "Transform each element: `fn(row) -> row`.",
    field: "Target a property on the current row.",
    set: "Queue a write on field or row.",
    create: "Emit one output pak for the given `:Value(...)`.",
  };

  monaco.languages.registerHoverProvider("lua", {
    provideHover(
      model: { getWordAtPosition: (p: { lineNumber: number; column: number }) => { word: string; startColumn: number; endColumn: number } | null },
      position: { lineNumber: number; column: number },
    ) {
      const word = model.getWordAtPosition(position);
      if (!word) return null;
      const doc = hoverDocs[word.word];
      if (!doc) return null;
      return {
        range: new monaco.Range(
          position.lineNumber,
          word.startColumn,
          position.lineNumber,
          word.endColumn,
        ),
        contents: [{ value: doc }],
      };
    },
  });
}
