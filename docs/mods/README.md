# How a mod runs

A UTool mod is a folder with `mod.lua`. The script does not run inside the game. It runs on your machine at build time. It reads assets out of the game's pak, changes them, and UnrealPak writes a new pak the game loads on top.

## Folder

```text
morexp/
  mod.lua          required. Calls utool.mod once, then the patches
  scripts/         only if mod.lua lists them, or if there is no mod.lua
  content/         optional loose files copied into the pak
  dist/            build output. Gitignored
  .cache/          extracted and prepared files. Gitignored
  media/           Nexus banner and header, from utool mod banner
```

`mod.json` is the old manifest. If both files exist, `mod.lua` wins. A folder with neither cannot build.

## What the script is allowed to call

Lua gets the standard `base`, `math`, `string`, `table`, and `package` libraries, plus a global `utool` table. There is no game API, no actors, and no tick. Anything you compute has to be done before the pak exists.

`utool.mod` may be called once. A second call throws `utool.mod may only be called once`. `id` and `name` are required.

## Load order

When `mod.lua` exists:

1. `mod.lua` runs to the end.
2. Each path in `utool.mod { scripts = { ... } }` runs, in order, unless it is `mod.lua` again.

The `scripts/` directory is not scanned in that case. A file you forgot to list does not run.

When there is no `mod.lua`, every `scripts/*.lua` runs in sorted path order. That path is for a leftover `mod.json` mod. New mods should use `mod.lua`.

## What "build" does

`utool mod build <folder>`:

1. Loads `utool.json` by walking up from the mod folder, and loads `.env` the same way.
2. Runs the scripts and collects the manifest plus every queued patch.
3. Extracts the assets those patches name from `sourcePak` (and curves from `curveSourcePak`).
4. Applies field writes, `:map` / `pak.create` transforms, `patch_asset` callbacks, and `patch_curve` callbacks.
5. Writes the result under `.cache/prepared`.
6. Runs UnrealPak. Output path comes from `pak.output`.
7. If `zip` is set, zips that pak.

`--force-extract` repeats the extract from the game pak. Use it after the game updates. Without it, a warm `.cache` can still hold the previous game build.

Patches are queued while the script runs. They are applied later, during the prepare step. A `:set` you can see in the script has not touched disk yet. A throw from a `:map` callback happens at prepare time and fails the build. The message includes `asset:map callback failed:` or `pak.create map failed:`.

## Install

`utool mod deploy <folder>` builds, then copies the pak into `<paksDir>/mods`. The game loads `*_P.pak` from that folder. One tier per save. Do not drop every zip from a multi-variant mod in at once.

## Where to read next

- Every manifest field: [Manifest](manifest.md)
- Change rows without a JSON pointer: [Data DSL](dsl.md)
- One script, many zips: [Variants](variants.md)
- JSON pointers, delete rows, rewrite a subtree: [Editor](editor.md)
- CurveFloat level caps: [Curves](curves.md)
- Why `{}` is not `[]`: [JSON rules](json.md)
- Complete mods: [Recipes](recipes.md)
