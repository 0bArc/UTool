# 08 — Pak Studio

Desktop UI for browsing paks, editing `mod.lua`, and building mods.

Location: [`tools/PakStudio`](../tools/PakStudio)

## Run

```powershell
cd tools/PakStudio
npm install
npm run studio
```

Or dev server:

```powershell
npm run dev
```

Requires Node.js. Uses `utool.exe` on PATH or configured in the studio environment.

## Typical session

1. **Welcome** — open existing project or create under `examples/`.
2. **Load game** — pick Icarus from `utool.json` or probe install folder.
3. **Editor** — edit `mod.lua`; browse assets in the sidebar.
4. **Build** — pack mod; log shows each pak/zip for multi-variant mods.
5. **Open folder** — reveals output in Explorer (selects zip when pak was removed after zip).

## VS Code

Extension command **UTool: Browse Game Paks** opens asset browse when configured.

## CLI parity

Anything Studio does for pack/list/preview maps to `utool pak …` commands in [04 — Find assets](04-find-assets.md).

## Back to start

[docs/README.md](README.md)
