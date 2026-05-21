# csmanager

[![.NET](https://img.shields.io/badge/.NET-8.0-512BD4?logo=dotnet&logoColor=white)](https://dotnet.microsoft.com/download/dotnet/8.0)
[![build](https://img.shields.io/github/actions/workflow/status/0bArc/csStratware/build.yml?label=build)](https://github.com/0bArc/csStratware/actions/workflows/build.yml)
[![platform](https://img.shields.io/badge/platform-Windows-0078D4?logo=windows&logoColor=white)](https://github.com/0bArc/csStratware)
[![UE4](https://img.shields.io/badge/engine-UE4%20%7C%20Icarus-333333)](https://github.com/0bArc/csStratware)

**csmanager** is a .NET 8 CLI for **UE4-style modding**: discover mods, patch game JSON (declarative or C#), and pack **`_P.pak`** overrides with **UnrealPak**. Built for games like **Icarus** that ship `data.pak` + mount-point overrides.

This repository (`csStratware` on GitHub) is the **source tree** for `csmanager` and its libraries (`CsStratware.*`). You install and run **`csmanager`**, not a binary named csStratware.

## Quick start

```powershell
git clone https://github.com/0bArc/csStratware.git
cd csStratware
dotnet run --project build.csproj -c Release
```

Add `dist\csmanager` to `PATH`, then:

```text
csmanager help
csmanager validate mods
csmanager compile mods\example-mod
csmanager pak build-mod <mod-dir>
```

Without PATH:

```powershell
dotnet build csStratware.sln -c Release
dist\csmanager\csmanager.exe help
```

## What it does

| Area | CLI / behavior |
|------|----------------|
| **Mods** | `list`, `validate`, `compile` — `mod.json`, C# `[PatchAsset]` / `[PatchPlayerData]`, JSON patches |
| **Paks** | `pak data pull/list`, `pak find`, `pak ue extract` (dir or `@paks`), build-mod — UnrealPak for `*_P.pak` |
| **Saves** | `playerdata` — local UE4 player data (e.g. accolades) |
| **Setup** | `setup unrealpak` — bundled `assets/UnrealPak.zip` or custom engine path |

Plain content-only packs can use the built-in C# `PakBuilder`; **Icarus-style overrides** need UnrealPak (`useUnrealPak` or `sourcePak` in `mod.json`).

## Solution layout

| Project | Role |
|---------|------|
| **CsStratware.Cli** | **`csmanager`** executable |
| **CsStratware.Sdk** | Mod author API — `AssetPatch`, `JsonAssetEditor`, player-data patches |
| **CsStratware.ModLoader** | Discover mods, apply patches, compile/run mod DLLs |
| **CsStratware.Pak** | Pak index/search, `ModAssetPreparer`, UnrealPak wrapper |
| **CsStratware.Infrastructure** | Cache, incremental builds, sandbox, parallel prepare |
| **CsStratware.Core** | `ModManifest`, shared models |

```
Cli → Pak, ModLoader → Infrastructure, Sdk → Core
```

Details: [src/README.md](src/README.md).

## Icarus walkthrough

Sibling repo **[csStratwareDemo](../csStratwareDemo)** — `mods/processor-850` sets every `RequiredMillijoules` to **850**. In-repo sample: [mods/example-mod/](mods/example-mod/). Step-by-step: [setup.md](setup.md).

## Config

Copy [csstratware.json.example](csstratware.json.example) → `csstratware.json` (gitignored).

| Key | Purpose |
|-----|---------|
| `unrealPak` / `unrealEngineDir` | Optional; default uses local `assets/UnrealPak.zip` (see [assets/README.md](assets/README.md)) |
| `icarusPaksDir`, `icarusDataPak` | Game paks (read/extract only) |
| `icarusMountPoint` | UE virtual mount in packed mods (`../../../Icarus/...` — game convention) |

**UnrealPak resolution** (first hit wins):

1. `<repo>/assets/UnrealPak/` — auto-extract from `assets/UnrealPak.zip` on first pack (zip not in git)
2. `<project>/tools/UnrealPak/Engine/` — `setup unrealpak --from …`
3. `%LocalAppData%\csmanager\UnrealPak\Engine\` — `setup unrealpak --appdata`
4. Legacy `C:\software\UnrealPak\` or Epic UE installs

Env: `CSSTRATWARE_UNREALPAK` (exe path), `CSSTRATWARE_ROOT` (repo root if auto-detect fails).

## Docs

- [src/README.md](src/README.md) — SDK, compile/pak flow, command reference
- [setup.md](setup.md) — Icarus processor mod from scratch
