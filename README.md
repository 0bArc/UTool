# csStratware

.NET 8 toolkit for UE4-style mods: discover mods, patch JSON (declarative or C#), pack `.pak` files (UnrealPak for game-ready overrides).

## Projects (all used)

| Project | Role |
|---------|------|
| **CsStratware.Core** | `ModManifest`, patch models, shared JSON helpers |
| **CsStratware.Sdk** | Mod author API — `AssetPatch`, `[PatchAsset]`, `JsonAssetEditor` |
| **CsStratware.ModLoader** | Discover mods, apply JSON patches, compile/run C# patch DLLs |
| **CsStratware.Pak** | Pak index/search, `ModAssetPreparer`, **UnrealPak** wrap, built-in `PakBuilder` for tooling |
| **CsStratware.Cli** | **`csmanager`** executable |

```
Cli → Pak, ModLoader → Sdk → Core
```

Icarus / UE4 override `*_P.pak` → **UnrealPak** (`useUnrealPak` or `sourcePak` in `mod.json`). Plain content-only packs can use the built-in C# `PakBuilder`.

## Build CLI

```powershell
cd F:\Data\personal\c#\csStratware
dotnet run --project build.csproj -c Release
```

Output: `dist/csmanager/csmanager.exe`. Add that folder to `PATH`, then:

```text
csmanager help
csmanager validate mods
csmanager compile mods\example-mod
csmanager pak build-mod <mod-dir>
```

Or without PATH:

```powershell
dotnet build csStratware.sln -c Release
dotnet run --project src\CsStratware.Cli\CsStratware.Cli.csproj -c Release -- validate mods
```

## Demo (Icarus)

Sibling repo **[csStratwareDemo](../csStratwareDemo)** — `mods/processor-850` sets every `RequiredMillijoules` to **850** (C# + optional JSON patch). See demo `README.md`.

## Docs

- [src/README.md](src/README.md) — Sdk reference, compile/pak flow, CLI commands
- [mods/example-mod/](mods/example-mod/) — in-repo sample (no game install)

## Config

Workspace `csstratware.json` (walks up from cwd): `unrealPak`, `icarusDataPak`, `icarusPaksDir`, …  
Env: `CSSTRATWARE_UNREALPAK` overrides `unrealPak`.
