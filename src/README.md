# csStratware source

.NET 8 UE4 mod toolkit: discover mods, patch JSON (declarative or C#), pack `.pak` files.

## Projects

| Project | Role |
|---------|------|
| **CsStratware.Core** | Models, JSON helpers |
| **CsStratware.Sdk** | Mod author API — `AssetPatch`, `JsonAssetEditor`, `[PatchAsset]` |
| **CsStratware.ModLoader** | `mod.json` discovery, JSON patches, compile & run C# patches |
| **CsStratware.Pak** | Pak index/search, UnrealPak wrap, `build-mod` prepare stage |
| **CsStratware.Cli** | **`csmanager`** executable (`list`, `validate`, `compile`, `pak`) |

Dependency flow: **Cli** → Pak, ModLoader → **Sdk** → Core.

## Build

```powershell
cd <repo-root>
dotnet run --project build.csproj -c Release
# PATH → dist\csmanager
csmanager help
```

Or: `dotnet build csStratware.sln -c Release`

## CsStratware.Sdk (mod code)

Reference from your mod `.csproj`:

```xml
<ProjectReference Include="..\..\..\csStratware\src\CsStratware.Sdk\CsStratware.Sdk.csproj" />
```

```csharp
using CsStratware.Sdk;

[PatchAsset("MyAsset.json")]
public sealed class MyPatch : AssetPatch
{
    public override void Apply(JsonAssetEditor editor)
    {
        editor.Replace("/some/path", 42);
        editor.ReplaceAll("SomeProperty", 500);
    }
}
```

Put project under `code/*.csproj` (or set `mod.json` → `codeProject`). CLI:

```text
csmanager compile <mod-dir>              # → .cache/compiled/*.dll
csmanager compile <mod-dir> --prepare    # + .cache/prepared/*.json
csmanager pak build-mod <mod-dir>        # compile + prepare + pack
```

Declarative patches: `patchFiles` → `patches/*.json` (same ops as `JsonAssetEditor`). Code + JSON patches merge per asset file name.

## Integration test (demo repo)

Full Icarus + Sdk path exercised in sibling **[csStratwareDemo](../../csStratwareDemo)**:

```powershell
cd F:\Data\personal\c#\csStratwareDemo
copy csstratware.json.example csstratware.json   # edit paths
csmanager validate mods
csmanager compile mods\processor-850
csmanager pak build-mod mods\processor-850
```

Covers: `validate`, `list`, `pak find`, `compile` (Sdk mod), `pak build-mod` (UnrealPak), `pak list`.

Demo mod: [csStratwareDemo](../../csStratwareDemo) `mods/processor-850` — `ReplaceAll("RequiredMillijoules", 850)` + UnrealPak pack.

## In-repo sample

`mods/example-mod/` — JSON patch + C# `GameplayPatch` (no game files required):

```powershell
csmanager validate mods
csmanager compile mods\example-mod
```

## CLI quick reference

```text
csmanager list|validate <mods-dir>
csmanager compile <mod-dir> [--prepare] [--force-extract]
csmanager pak find <dir|@icarus> <needle> [--path-only]
csmanager pak build-mod <mod-dir> [-o out.pak]
csmanager pak list|extract|ue extract|pack ...
```

Game paths: workspace `csstratware.json` (demo) or env; Icarus shortcuts `@icarus`, `@icarus-data`.

### UnrealPak setup

Icarus override paks need **Icarus Mod Manager’s** UnrealPak (not Epic’s generic build). Copy once:

```powershell
csmanager setup unrealpak    # uses csstratware.json unrealEngineDir
csmanager setup help
```

Local copy → `tools/UnrealPak/` (project) or `%LocalAppData%\csmanager\UnrealPak\`. See root [README](../README.md#unrealpak-required-for-icarus-_p_pak-mods).
