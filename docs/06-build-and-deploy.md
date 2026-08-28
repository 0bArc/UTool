# 06 — Build and deploy

One command builds everything declared in `mod.lua`.

## Build

```powershell
utool pak build-mod <mod-dir>
```

Examples:

```powershell
utool pak build-mod examples/morexp
utool pak build-mod examples/beasttamer --force-extract
cd examples/freshrations
utool pak build-mod .
```

| Flag | When |
|------|------|
| `--force-extract` | Re-pull JSON from `data.pak` (game update) |
| `-compress` | Smaller pak, slower build |
| `-o out.pak` | Single output override — **avoid** for multi-variant mods |

No extra CLI flags for `:Value` or `:zip` — the Lua script handles variants.

## Output

- Pak path comes from `pak.output` in `mod.lua` (`%updateversion%`, `%d` — see [07](07-multi-variant-mods.md)).
- With `zip = true` or `:zip(...)`, you get `.zip` files in `dist/` and the `.pak` is deleted after zipping.
- Install **one** zip/pak from a tier pack, not all of them.

## Deploy to game folder

```powershell
utool deploy examples/morexp
```

Copies the built `*_P.pak` into `Content/Paks/mods/` for the configured game.

Manual install: drop pak or unzip into:

```text
<Icarus>/Icarus/Content/Paks/mods/
```

## Validate before build

```powershell
utool validate examples/morexp
utool discover examples
```

## Troubleshooting

**UnrealPak not found** — ensure `assets/UnrealPak.zip` exists or set `UTOOL_UNREALPAK`.

**Check fails** — fix `utool.json` paths; run `utool check Icarus`.

**Build succeeds but game ignores mod** — wrong mount point or asset path; compare with a working example mod.

**Curve patch rejected by game** — large key-array edits rewrite `.uexp`; re-save with UAssetGUI or supply a sidecar JSON.

**No assets to prepare** — mod needs patches in `mod.lua` / scripts and valid `sourcePak`.

## Next

Multi-tier packs: [07 — Multi-variant mods](07-multi-variant-mods.md)
