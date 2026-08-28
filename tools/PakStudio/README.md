# Pak Studio

Next.js browser UI for the UTool pak debugger and Lua mod pipelines. Dark theme aligned with [stratware.win](https://stratware.win): black background, Inter font, flat borders. Monaco edits existing `mod.lua` mods beside pak previews.

## Layout

- **Pipelines** — registry under `pipelines/` pointing at `examples/*/mod.lua` (no new mod format)
- **Paks / folders** — inventory browser
- **Build** — pak artifact; **Deploy** copies `*_P.pak` → `Content/Paks/mods`
- **Monaco** — edit `mod.lua` / scripts; **Insert snippet** from assets
- **Asset panel** — `utool pak open` (LRU extract cache ≤256 MB) with string unbox

## Deep search without bulk disk

- **Paths** — virtual path / pak name filter
- **Inside files** — durable string index (`%LOCALAPPDATA%/utool/cache/pak-strings`), built with **single-file** extract (not whole-pak dumps). Skips bodies &gt; 8 MB by default.

Open cache: `%LOCALAPPDATA%/utool/cache/pak-open/`.

## Game selection

1. **Configured games**: buttons from `examples/utool.json` (or repo-root `utool.json`).
2. **Install folder**: paste e.g. `D:\Games\Pacific Drive` and **Scan**.

```powershell
$env:UTOOL_CONFIG_DIR = "F:\path\to\folder-with-utool.json"
```

## Run

```powershell
cd tools/PakStudio
npm install
$env:UTOOL_EXE = "F:\Data\personal\utool-build\utool.exe"   # optional
.\run-pak-studio.cmd
# or: utool pak studio
```

Opens http://127.0.0.1:3000 (or your `--port`). Pak ops go through `POST /api/utool` → `utool pak … --json`.

### Why `run-pak-studio.cmd`?

This repo lives under `c#\csStratware`. Next.js mishandles `#` in paths. The launcher maps `P:` → `tools/PakStudio` via `subst`.

```powershell
cd tools/PakStudio
npm run dev -- --port 4002
```

## Pipelines

See [pipelines/README.md](pipelines/README.md). **Build** runs `utool pak build-mod <mod-dir>` on the selected pipeline.

## CLI used by Studio

```
utool pak list|search|open|preview|snippet|extract|build-mod
utool pak search <q> --from <src> --inside   # string index
utool pak open <path> --from <src> --pak …   # LRU cache extract + unbox
```

## Requirements

- Node.js 18+
- Windows (for `subst` launcher)
- Built `utool.exe` on PATH, or `UTOOL_EXE`, or `%LOCALAPPDATA%\utool\utool.exe`
