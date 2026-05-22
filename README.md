# UTool

[![.NET](https://img.shields.io/badge/.NET-8.0-512BD4?logo=dotnet&logoColor=white)](https://dotnet.microsoft.com/download/dotnet/8.0)
[![build](https://img.shields.io/github/actions/workflow/status/0bArc/utool/build.yml?label=build)](https://github.com/0bArc/utool/actions/workflows/build.yml)
[![platform](https://img.shields.io/badge/platform-Windows-0078D4?logo=windows&logoColor=white)](https://github.com/0bArc/utool)
[![UE](https://img.shields.io/badge/engine-UE4%20%7C%20UE5-333333)](https://github.com/0bArc/utool)

**UTool** — modding toolkit for extracting, patching, rebuilding, and packaging UE4/UE5 game assets, including pak files, JSON data, CurveFloat assets, and player data.

This repository is the **source tree** for the `utool` CLI and its libraries (`UTool.*`). Install and run **`utool`**, not a binary named after the repo folder.

## Quick start

```powershell
git clone https://github.com/0bArc/utool.git
cd utool
dotnet run --project build.csproj -c Release
```

Add `dist\utool` to `PATH`, then:

```text
utool help
utool validate mods
utool compile mods\example-mod
utool pak build-mod <mod-dir>
```

Without PATH:

```powershell
dotnet build utool.sln -c Release
dist\utool\utool.exe help
```

## What it does

| Area | CLI / behavior |
|------|----------------|
| **Mods** | `list`, `validate`, `compile` — `mod.json`, C# `[PatchAsset]` / `[PatchPlayerData]`, JSON patches |
| **Paks** | `pak data pull/list`, `pak find`, `pak ue extract` (dir or `@paks`), build-mod — UnrealPak for `*_P.pak` |
| **Saves** | `playerdata` — local UE4 player data (e.g. accolades) |
| **Setup** | `setup unrealpak` — bundled `assets/UnrealPak.zip` or custom engine path |

Plain content-only packs can use the built-in C# `PakBuilder`; **mount-point overrides** need UnrealPak (`useUnrealPak` or `sourcePak` in `mod.json`).

## Solution layout

| Project | Role |
|---------|------|
| **UTool.Cli** | **`utool`** executable |
| **UTool.Sdk** | Mod author API — `AssetPatch`, `JsonAssetEditor`, player-data patches |
| **UTool.ModLoader** | Discover mods, apply patches, compile/run mod DLLs |
| **UTool.Pak** | Pak index/search, `ModAssetPreparer`, UnrealPak wrapper |
| **UTool.Infrastructure** | Cache, incremental builds, sandbox, parallel prepare |
| **UTool.Core** | `ModManifest`, shared models |

```
Cli → Pak, ModLoader → Infrastructure, Sdk → Core
```

Details: [src/README.md](src/README.md).

## Config

Copy [utool.json.example](utool.json.example) → `utool.json` (gitignored). Legacy `csstratware.json` is still read if present.

| Key | Purpose |
|-----|---------|
| `unrealPak` / `unrealEngineDir` | Optional; default uses local `assets/UnrealPak.zip` (see [assets/README.md](assets/README.md)) |
| `gamePaksDir`, `dataPak` | Game paks (read/extract only) |
| `defaultMountPoint` | UE virtual mount in packed mods |

**UnrealPak resolution** (first hit wins):

1. `<repo>/assets/UnrealPak/` — auto-extract from `assets/UnrealPak.zip` on first pack
2. `<project>/tools/UnrealPak/Engine/` — `setup unrealpak --from …`
3. `%LocalAppData%\utool\UnrealPak\Engine\` — `setup unrealpak --appdata`
4. Legacy `C:\software\UnrealPak\` or Epic UE installs

Env: `UTOOL_UNREALPAK` (exe path), `UTOOL_ROOT` (repo root if auto-detect fails).

## Docs

- [src/README.md](src/README.md) — SDK, compile/pak flow, command reference
- [setup.md](setup.md) — game mod walkthrough from scratch
