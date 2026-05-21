# Setup: Icarus processor mod (850 millijoules)

This guide is for people new to csStratware who want a mod that changes **every** `RequiredMillijoules` value in the game’s processor recipes to **850**.

In Icarus, that field is the “energy cost” on bio processor recipes (stored in `D_ProcessorRecipes.json` inside `data.pak`). You do not edit the `.pak` by hand — you describe the change, and `csmanager` rebuilds a game-ready `*_P.pak`.

---

## What you need

| Thing | Why |
|-------|-----|
| **.NET 8 SDK** | Builds `csmanager` and your mod’s small C# patch DLL |
| **Icarus (Steam)** | Game files; `data.pak` is the recipe source |
| **UnrealPak** | Local `assets/UnrealPak.zip` or `setup unrealpak --from` — see [assets/README.md](assets/README.md) |
| **This repo built** | Gives you the `csmanager` CLI |

Optional but easiest: use the sibling demo workspace **[csStratwareDemo](../csStratwareDemo)** — it already contains a working `mods/processor-850` mod.

---

## One-time: build `csmanager`

From the **csStratware** repo root:

```powershell
cd F:\Data\personal\c#\csStratware
dotnet run --project build.csproj -c Release
```

Add the CLI to your PATH for this session:

```powershell
$env:PATH = "F:\Data\personal\c#\csStratware\dist\csmanager;" + $env:PATH
csmanager help
```

---

## One-time: point at your game

In **csStratwareDemo** (recommended) or any workspace that will run `pak build-mod`:

```powershell
cd F:\Data\personal\c#\csStratwareDemo
copy csstratware.json.example csstratware.json
```

Edit `csstratware.json` — at minimum set:

- `icarusDataPak` — path to `Icarus/Content/Data/data.pak` (see [csstratware.json.example](csstratware.json.example))

UnrealPak: put **`assets/UnrealPak.zip`** in this repo, or point at an Epic UE install. First pack extracts to `assets/UnrealPak/`. Optional:

```powershell
csmanager setup unrealpak
```

---

## Fastest path: use the demo mod

```powershell
cd F:\Data\personal\c#\csStratwareDemo
csmanager validate mods
csmanager compile mods\processor-850
csmanager pak build-mod mods\processor-850
```

**Output:** `mods/processor-850/dist/processor-850_P.pak`

Copy that `*_P.pak` into your Icarus mods folder. In game, processor recipes should all use **850** mJ.

Sanity checks:

```powershell
csmanager pak find @icarus ProcessorRecipes --path-only --max 5
csmanager pak list mods\processor-850\dist\processor-850_P.pak
```

---

## Build the same mod yourself (from scratch)

Use a **workspace folder** that has `csstratware.json` next to a `mods/` directory (the demo repo is ideal). Create:

```
mods/my-processor-850/
  mod.json
  code/
    MyProcessor850.csproj
    MyProcessor850Patch.cs
```

### 1. `mod.json`

```json
{
  "id": "icarus.processor.850mj",
  "name": "Processor 850 mJ",
  "version": "1.0.0",
  "description": "Sets every RequiredMillijoules to 850.",
  "author": "you",
  "target": {
    "gameId": "Icarus",
    "engineVersion": "4.27",
    "minGameVersion": "1.0.0"
  },
  "contentRoots": [],
  "patchFiles": [],
  "codeProject": "code/MyProcessor850.csproj",
  "pak": {
    "output": "dist/my-processor-850_P.pak",
    "mountPoint": "../../../Icarus/Content/data/Crafting/",
    "sourcePak": "@icarus-data",
    "useUnrealPak": true
  }
}
```

Important fields:

- **`sourcePak": "@icarus-data"`** — reads `D_ProcessorRecipes.json` from game `data.pak` during prepare (path from `csstratware.json`).
- **`mountPoint`** — where the patched file must live inside the pak so Icarus loads it (crafting data tree).
- **`useUnrealPak": true`** — required for real Icarus override paks.

### 2. C# patch (`code/MyProcessor850Patch.cs`)

```csharp
using CsStratware.Sdk;

[PatchAsset("D_ProcessorRecipes.json")]
public sealed class MyProcessor850Patch : AssetPatch
{
    public override void Apply(JsonAssetEditor editor) =>
        editor.ReplaceAll("RequiredMillijoules", 850);
}
```

`[PatchAsset("D_ProcessorRecipes.json")]` must match the **file name** inside the pak, not a UE asset path like `/Game/...`.

### 3. Project file (`code/MyProcessor850.csproj`)

Point `ProjectReference` at **your** csStratware clone:

```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <TargetFramework>net8.0</TargetFramework>
    <ImplicitUsings>enable</ImplicitUsings>
    <Nullable>enable</Nullable>
  </PropertyGroup>
  <ItemGroup>
    <ProjectReference Include="..\..\..\..\csStratware\src\CsStratware.Sdk\CsStratware.Sdk.csproj" />
  </ItemGroup>
</Project>
```

Adjust the number of `..\` segments so the path reaches your `CsStratware.Sdk.csproj`.

### 4. Build

```powershell
csmanager validate mods
csmanager compile mods\my-processor-850
csmanager pak build-mod mods\my-processor-850
```

---

## JSON-only version (no C#)

If you prefer not to compile C#, use a patch file instead.

`patches/processor-recipes.json`:

```json
{
  "patches": [
    {
      "assetPath": "D_ProcessorRecipes.json",
      "operations": [
        { "op": "replaceAll", "path": "/RequiredMillijoules", "value": 850 }
      ]
    }
  ]
}
```

In `mod.json`:

- Set `"patchFiles": ["patches/processor-recipes.json"]`
- Remove `"codeProject"`

Then run `csmanager pak build-mod` (no `compile` needed unless you also have C#).

---

## How it works (short)

```
data.pak (game)
    → extract D_ProcessorRecipes.json to .cache/source/
    → apply patch (C# ReplaceAll or JSON replaceAll)
    → write .cache/prepared/D_ProcessorRecipes.json
    → UnrealPak packs it → dist/*_P.pak
```

You change **one property name** everywhere in that JSON tree. The tool ships the **whole** patched JSON file inside the pak at the correct mount path.

---

## Troubleshooting

| Problem | What to try |
|---------|-------------|
| `csmanager` not found | Build csStratware; add `dist\csmanager` to PATH |
| UnrealPak / prepare fails | Add `assets/UnrealPak.zip` or run `csmanager setup unrealpak --from <UE Engine>` |
| Patch does nothing in game | Confirm output is `*_P.pak`, mount point matches crafting data, mod enabled in Icarus |
| Wrong file patched | `pak find @icarus RequiredMillijoules` — asset file name must match `[PatchAsset(...)]` |
| Want to inspect JSON | `csmanager pak ue extract <data.pak> extracted --filter *D_ProcessorRecipes*` |

---

## Learn more

- Working sample: [csStratwareDemo/mods/processor-850](../csStratwareDemo/mods/processor-850)
- In-repo toy sample (no game): [mods/example-mod/](mods/example-mod/)
- CLI & architecture: [src/README.md](src/README.md)
- Root overview: [README.md](README.md)
