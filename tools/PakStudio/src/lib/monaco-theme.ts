import type { Monaco } from "@monaco-editor/react";

export function definePakStudioTheme(monaco: Monaco): void {
  monaco.editor.defineTheme("pak-studio", {
    base: "vs-dark",
    inherit: true,
    rules: [],
    colors: {
      "editor.background": "#000000",
      "editor.foreground": "#ffffff",
      "editor.lineHighlightBackground": "#111111",
      "editor.selectionBackground": "#ffffff33",
      "editor.inactiveSelectionBackground": "#ffffff22",
      "editor.selectionHighlightBackground": "#ffffff18",
      "editorCursor.foreground": "#ffffff",
      "editorLineNumber.foreground": "#6b7280",
    },
  });
}
