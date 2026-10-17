# UTool manual

UTool builds Unreal Engine mods from Lua. The pages below are the manual. Start at setup, then the mod language, then the CLI.

## Setup

| Page | Covers |
|------|--------|
| [Install](setup/install.md) | Build `utool.exe`, PATH |
| [Configure](setup/configure.md) | `utool.json`, aliases, catalog |
| [UnrealPak](setup/unrealpak.md) | Where the packer comes from |

## Mods

The language reference. Read [How a mod runs](mods/README.md) once, then the page for the call you are using.

| Page | Covers |
|------|--------|
| [How a mod runs](mods/README.md) | Files, load order, what build does |
| [Manifest](mods/manifest.md) | `utool.mod { }` every field |
| [Data DSL](mods/dsl.md) | `utool.asset`, `:row`, `:field`, `:map`, `:find` |
| [Variants](mods/variants.md) | `pak.create`, `:Value`, `:zip`, `%d`, `%updateversion%` |
| [Editor](mods/editor.md) | `patch_asset` and `utool.editor` |
| [Curves](mods/curves.md) | `patch_curve`, keys, level caps |
| [JSON rules](mods/json.md) | Arrays vs objects, empty `[]` |
| [Recipes](mods/recipes.md) | Full mods: one field, multiplier, tiers, removal |

## CLI

| Page | Covers |
|------|--------|
| [Commands](cli/commands.md) | `utool mod`, `utool pak`, menus |
| [Ship](cli/ship.md) | Week builds and Nexus upload |
