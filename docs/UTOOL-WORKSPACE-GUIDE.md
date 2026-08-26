# UTool workspace guide

## Build-mod flow

```text
mod.json
  → load scripts (Lua) + patchFiles
  → prepare JSON assets (extract → patch → .cache/prepared)
  → prepare curves (vanilla uasset → Lua CurveEditor → native key rewrite → prepared)
  → merge content/ + prepared
  → UnrealPak pack → dist/*_P.pak
```

## Level 250 cap example

Path: `examples/250cap`

- `scripts/extend_progression.lua` — extends XP/talent/blueprint/solo curves to level 250
- `scripts/level_display_cap.lua` — sets `MaxDisplayLevel` on `D_CharacterGrowth`

Requires `utool.json` with Icarus `@paks` / `@data` paths.

```powershell
utool pak build-mod examples/250cap
```

## mod.json

| Field | Role |
|-------|------|
| `scripts` | Lua files registering `utool.patch_curve` / `utool.patch_asset` |
| `patchFiles` | Declarative JSON patch documents |
| `contentRoots` | Extra files packed as-is |
| `pak.mountPoint` | UnrealPak mount prefix |
| `pak.sourcePak` | JSON source (`@data` alias OK) |
| `pak.curveSourcePak` | CurveFloat source (`@paks` alias OK) |

`codeProject` is removed. Mods are Lua, not C#.

## Config aliases

| Alias | Resolves |
|-------|----------|
| `@data` / `@icarus-data` | `dataPak` |
| `@paks` / `@icarus` | all `*.pak` under `paksDir` |

## Troubleshooting

- **UnrealPak not found** — place `assets/UnrealPak.zip` in the repo (extracts under `assets/UnrealPak/`) or set `UTOOL_UNREALPAK`.
- **Curve binary patch note** — growing key arrays rewrites `.uexp`. If the game rejects the asset, re-save with UAssetGUI or supply a UAssetAPI JSON sidecar (`*.uasset.json`).
- **No assets to prepare** — need `scripts`, `patchFiles`, or `curves/*.curve.json`, plus configured source paks.
