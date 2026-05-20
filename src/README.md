# csStratware source

.NET 8 UE4 mod toolkit: discover mods, patch JSON (declarative or C#), pack `.pak` files.

## Projects

| Project | Role |
|---------|------|
| **CsStratware.Core** | Models, JSON helpers |
| **CsStratware.Infrastructure** | Caching, incremental builds, logging, parallel pipelines, mod sandbox hooks |
| **CsStratware.Sdk** | Mod author API — `AssetPatch`, `ConditionalAssetPatch`, `PlayerDataPatch`, `JsonAssetEditor` |
| **CsStratware.ModLoader** | `mod.json` discovery, JSON patches, compile & run C# patches |
| **CsStratware.Pak** | Pak index/search, UnrealPak wrap, `build-mod` prepare stage |
| **CsStratware.Cli** | **`csmanager`** executable (`list`, `validate`, `compile`, `pak`) |


Dependency flow: **Cli** → Pak, ModLoader → **Infrastructure**, **Sdk** → Core.

## CsStratware.Infrastructure

Shared performance, cache, and safety layer used by **Pak** and **ModLoader**. Mod authors normally do not reference this project directly.

| Area | Types / behavior |
|------|------------------|
| **Caching** | `ContentHasher` (SHA-256), `AssetIndexCache` (filename → path index for extracted trees), `ExtractionCache` (validates UnrealPak extractions by manifest hash), `SharedCacheStore` (`%LocalAppData%\csmanager\cache` + per-mod `.cache/shared`) |
| **IO** | `StreamingFileOps` — async read/write, hardlink-or-copy for large merges |
| **Build** | `IncrementalBuildTracker` — skip `prepare` when inputs/outputs unchanged; `ModBuildGraph` — ordered async build steps |
| **Operations** | `OperationContext`, `OperationProgress` — cancellation + `--progress` reporting |
| **Logging** | `StratwareLog` — structured levels, timed scopes (`--verbose` / `-v` on CLI) |
| **Pipeline** | `ParallelPatchPipeline` — parallel per-asset prepare |
| **Security** | `ModAssemblySandbox` — collectible `AssemblyLoadContext`, blocks `System.Net.*`, optional Sdk version check |
| **Mods** | `ModConflictResolver` — duplicate JSON pointer detection across patch sources |
| **Validation** | `JsonSchemaValidator` — lightweight FModel/UE export JSON sanity checks |

**Pak** builds on Infrastructure with: `PakArchiveCache` (reuse open pak indexes), `StreamingPakGrep` (chunked content search), `UnrealPakExtractionPipeline` (deduped extracts), `PakOpenOptions` / AES index decrypt (`--aes-key`, `PAK_AES_KEY`), Oodle detection with clear fallback to UnrealPak, `IoStoreSupport` placeholder for UE5.

Per-mod cache layout (under `<mod>/.cache/`):

| Path | Purpose |
|------|---------|
| `source/` | Cached game JSON + `.sha256` sidecar (replaces size-only validity) |
| `prepared/` | Patched JSON staged for pack |
| `compiled/` | Mod C# patch DLL |
| `ue-extract/` | Filtered UnrealPak output (shared extraction cache also applies) |
| `build-prepare.json` | Incremental prepare state |
| `asset-index.json` | Indexed file list when scanning extracted dirs |

## Build

```powershell
cd <repo-root>
dotnet run --project build.csproj -c Release
# PATH → dist\csmanager
csmanager help
```

Or: `dotnet build csStratware.sln -c Release`

Tests: `dotnet test tests/CsStratware.Tests -c Release`

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
        editor.ReplaceAll("SomeProperty", 500);           // whole tree
        editor.ReplaceAll("SomeProperty", 500, "/0/Properties");  // scoped subtree
    }
}
```

Put project under `code/*.csproj` (or set `mod.json` → `codeProject`). CLI:

```text
csmanager compile <mod-dir>              # → .cache/compiled/*.dll
csmanager compile <mod-dir> --prepare    # + .cache/prepared/*.json
csmanager pak build-mod <mod-dir>        # compile + prepare + pack
```

Optional declarative patches: `patchFiles` → `patches/*.json`. Prefer code-only `[PatchAsset]` / `[PatchPlayerData]`.

### PlayerData (local UE4 saves)

Default root: `%LocalAppData%/<GameId>/Saved/PlayerData` (Icarus → `...\Icarus\Saved\PlayerData`). Override in `csstratware.json`:

```json
{ "icarusPlayerDataDir": "C:\\Users\\you\\AppData\\Local\\Icarus\\Saved\\PlayerData" }
```

Or env `CSSTRATWARE_PLAYER_DATA`.

```csharp
[PatchAsset("D_AICreatureType.json")]
public sealed class MyPatch : ConditionalAssetPatch
{
    public override bool ShouldApply(IPlayerSaveContext saves) =>
        saves.AnyProfileHasCompletedAccolade("DefeatQuarrite");

    public override void Apply(JsonAssetEditor editor) { /* pak JSON */ }
}

[PatchPlayerData("BestiaryData.json")]
public sealed class SavePatch : PlayerDataPatch
{
    public override void Apply(JsonAssetEditor editor, PlayerDataApplyContext ctx) { /* per-profile save */ }
}
```

```text
csmanager playerdata list
csmanager playerdata status <mod-dir>     # gate / skip asset patches
csmanager playerdata apply <mod-dir>      # write [PatchPlayerData] to saves
```

`compile --prepare` / `pak build-mod` read PlayerData for `ConditionalAssetPatch`; if gate fails, asset not staged (no pak change).

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
csmanager pak find <dir|@icarus> <needle> [--path-only] [--grep] [--aes-key <hex>] [--progress] [-v]
csmanager pak build-mod <mod-dir> [-o out.pak] [--force-extract] [--progress] [-v]
csmanager pak list|extract|grep|ue extract|pack ...
```

| Flag | Effect |
|------|--------|
| `--progress` | Step progress on stderr (prepare, parallel patch) |
| `-v` / `--verbose` | `StratwareLog` debug output |
| `--aes-key` / `PAK_AES_KEY` | AES-256 key for encrypted pak index |
| `--grep` | `pak find` also searches entry bytes (path search is default) |
| `--force-extract` | Ignore incremental + extraction caches |

Game paths: workspace `csstratware.json` (demo) or env; Icarus shortcuts `@icarus`, `@icarus-data`.

### UnrealPak setup

Bundled **`assets/UnrealPak.zip`** → **`assets/UnrealPak/`** on first use:

```powershell
csmanager setup unrealpak
```

Also `tools/UnrealPak/`, `%LocalAppData%\csmanager\UnrealPak\`, legacy `C:\software\UnrealPak`. See root [README](../README.md#unrealpak-icarus-_p_pak-mods).
