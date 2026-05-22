# UTool source

Modding toolkit for UE4/UE5: discover mods, patch JSON (declarative or C#), pack `.pak` files, CurveFloat assets, and player data.

## Projects

| Project | Role |
|---------|------|
| **UTool.Core** | Models, JSON helpers |
| **UTool.Infrastructure** | Caching, incremental builds, logging, parallel pipelines, mod sandbox hooks |
| **UTool.Sdk** | Mod author API — `AssetPatch`, `ConditionalAssetPatch`, `PlayerDataPatch`, `JsonAssetEditor` |
| **UTool.ModLoader** | `mod.json` discovery, JSON patches, compile & run C# patches |
| **UTool.Pak** | Pak index/search, UnrealPak wrap, `build-mod` prepare stage |
| **UTool.Cli** | **`utool`** executable (`list`, `validate`, `compile`, `pak`) |


Dependency flow: **Cli** → Pak, ModLoader → **Infrastructure**, **Sdk** → Core.

## UTool.Infrastructure

Shared performance, cache, and safety layer used by **Pak** and **ModLoader**. Mod authors normally do not reference this project directly.

| Area | Types / behavior |
|------|------------------|
| **Caching** | `ContentHasher` (SHA-256), `AssetIndexCache` (filename → path index for extracted trees), `ExtractionCache` (validates UnrealPak extractions by manifest hash), `SharedCacheStore` (`%LocalAppData%\utool\cache` + per-mod `.cache/shared`) |
| **IO** | `StreamingFileOps` — async read/write, hardlink-or-copy for large merges |
| **Build** | `IncrementalBuildTracker` — skip `prepare` when inputs/outputs unchanged; `ModBuildGraph` — ordered async build steps |
| **Operations** | `OperationContext`, `OperationProgress` — cancellation + `--progress` reporting |
| **Logging** | `UToolLog` — structured levels, timed scopes (`--verbose` / `-v` on CLI) |
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
# PATH → dist\utool
utool help
```

Or: `dotnet build utool.sln -c Release`

Tests: `dotnet test tests/UTool.Tests -c Release`

## UTool.Sdk (mod code)

Reference from your mod `.csproj`:

```xml
<!-- adjust ..\ segments to reach your utool clone -->
<ProjectReference Include="..\..\..\src\UTool.Sdk\UTool.Sdk.csproj" />
```

```csharp
using UTool.Sdk;

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
utool compile <mod-dir>              # → .cache/compiled/*.dll
utool compile <mod-dir> --prepare    # + .cache/prepared/*.json
utool pak build-mod <mod-dir>        # compile + prepare + pack
```

Optional declarative patches: `patchFiles` → `patches/*.json`. Prefer code-only `[PatchAsset]` / `[PatchPlayerData]`.

### PlayerData (local UE4 saves)

Default root: `%LocalAppData%/<GameId>/Saved/PlayerData` (Icarus → `...\Icarus\Saved\PlayerData`). Override in `utool.json`:

```json
{ "icarusPlayerDataDir": "C:\\Users\\you\\AppData\\Local\\Icarus\\Saved\\PlayerData" }
```

Or env `UTOOL_PLAYER_DATA`.

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
utool playerdata list
utool playerdata status <mod-dir>     # gate / skip asset patches
utool playerdata apply <mod-dir>      # write [PatchPlayerData] to saves
```

`compile --prepare` / `pak build-mod` read PlayerData for `ConditionalAssetPatch`; if gate fails, asset not staged (no pak change).

## Integration test (demo repo)

Full Icarus + Sdk path exercised in sibling **[utoolDemo](../../utoolDemo)**:

```powershell
cd F:\Data\personal\c#\utoolDemo
copy utool.json.example utool.json   # edit paths
utool validate mods
utool compile mods\processor-850
utool pak build-mod mods\processor-850
```

Covers: `validate`, `list`, `pak find`, `compile` (Sdk mod), `pak build-mod` (UnrealPak), `pak list`.

Demo mod: [utoolDemo](../../utoolDemo) `mods/processor-850` — `ReplaceAll("RequiredMillijoules", 850)` + UnrealPak pack.

## In-repo sample

`mods/example-mod/` — JSON patch + C# `GameplayPatch` (no game files required):

```powershell
utool validate mods
utool compile mods\example-mod
```

## CLI quick reference

```text
utool list|validate <mods-dir>
utool compile <mod-dir> [--prepare] [--force-extract]
utool pak find <dir|@icarus> <needle> [--path-only] [--grep] [--aes-key <hex>] [--progress] [-v]
utool pak build-mod <mod-dir> [-o out.pak] [--force-extract] [--progress] [-v]
utool pak list|extract|grep|data list|data pull|ue extract|ue pack ...
```

| Flag | Effect |
|------|--------|
| `--progress` | Step progress on stderr (prepare, parallel patch) |
| `-v` / `--verbose` | `UToolLog` debug output |
| `--aes-key` / `PAK_AES_KEY` | AES-256 key for encrypted pak index |
| `--grep` | `pak find` also searches entry bytes (path search is default) |
| `--force-extract` | Ignore incremental + extraction caches |

Game paths: workspace `utool.json` (legacy `csstratware.json` still read). Aliases: `@data`, `@icarus-data`, `@paks`, `@icarus` (see `UToolConfig`).

### UnrealPak setup

Bundled **`assets/UnrealPak.zip`** → **`assets/UnrealPak/`** on first use:

```powershell
utool setup unrealpak
```

Also `tools/UnrealPak/`, `%LocalAppData%\utool\UnrealPak\`, legacy `C:\software\UnrealPak`. See root [README](../README.md#config).
