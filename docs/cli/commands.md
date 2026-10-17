# Commands

`utool help`, `utool mod help`, and `utool pak help` print what the binary accepts. If this page and the help text disagree, trust the binary.

No arguments in a terminal opens a menu: `mods`, `pak`, `..`, `exit`. A pipe with no arguments prints help instead, so `utool | more` does not sit in the menu.

`-f` / `--find` matches a catalog id, the folder name, or the mod display name. Several matches print a message telling you to narrow the query. Catalog id is the folder key (`morexp`). The Nexus page number is a different id. `utool mod link` is what connects them.

## mod

| Command | Effect |
|---------|--------|
| `utool mod new` | Wizard. Needs a `utool.json` above the working directory |
| `utool mod list [mods-dir]` | List mods. Default directory is `examples` when that folder exists |
| `utool mod validate [mods-dir]` | Load each manifest. A bad `mod.lua` prints the Lua error |
| `utool mod open -f <query>` | Menu for one mod |
| `utool mod build <mod-dir>` | Pack what `mod.lua` queued |
| `utool mod build -f <query>` | Same, after resolving the folder |
| `utool mod deploy <mod-dir>` | Build, then copy the pak to `<paksDir>\mods` |
| `utool mod banner -f <query>` | Write `media/banner_1300x372.png` and `media/header_1400x400.png` |
| `utool mod banner --all` | Banners for every catalog mod |
| `utool mod link -f <query> <nexusModId>` | Set `nexus.files[].modId` for that catalog id |
| `utool mod ship` | [Ship](ship.md) |

### mod new

The first prompt is whether the mod is already on disk. No means scaffold.

Scaffold prompts: display name, catalog id (default is a slug of the name), category (`survival`, `inventory`, `creatures`, `progression`, `workshop`, `dev`, `miscellaneous`, or any other slug), description, author, game id, whether to add a ship catalog row, optional Nexus page id, and template.

Template `blank` writes an empty `mod.lua`. Template `scale` (also `scale-field` or `tiers`) asks for asset file names and one numeric field, then writes a multi-tier stub that divides that field. The folder is `<mods-dir>/<category>/<catalog-id>/`. The wizard then opens the mod menu.

Yes on "already in examples" asks for the catalog id, can add a missing catalog row, can link a Nexus id, and opens the menu. It does not overwrite `mod.lua`.

### Mod menu

`build`, `deploy`, `banner`, `link <nexusModId>`, `ship dry-run|upload [week]`, `..`, `exit`. `help` prints that list.

### Build flags

| Flag | Effect |
|------|--------|
| `--force-extract` | Re-read assets from the game pak |
| `--update-version N` | `%updateversion%` for this run only |
| `--compress` | Pass compression to UnrealPak |
| `-o` / `--output` | Force one output path. Skip this on a multi-variant mod |
| `--mount` | Override `mountPoint` for this run |

Old names still forward to `utool mod`: `utool ship`, `utool deploy`, `utool pak build-mod`, `utool new mod`, `utool banner`.

### Banners

```powershell
pip install -r scripts/banner/requirements.txt
utool mod banner -f mymod
```

Pillow missing fails with `banner failed` and that install line. Nexus does not take these files from the zip. Upload them on the mod's Images tab.

## pak

A source is `@data`, `@paks`, a game id (`Icarus`), or a path to one `.pak`.

```powershell
utool pak list @paks --ext json --limit 50 --offset 0
utool pak search CharacterGrowth --from Icarus --ext json
utool pak search Stamina --from @data --inside
utool pak preview Data/Character/D_CharacterGrowth.json --from @data
utool pak open Data/Character/D_CharacterGrowth.json --from @data --json
utool pak snippet Data/Character/D_CharacterGrowth.json --from @data --row Player --field MaxDisplayLevel
utool pak extract Data/Character/D_CharacterGrowth.json --from @data -o out
```

| Command | Effect |
|---------|--------|
| `pak list` | Paths in a pak. `--ext` filters. `--offset` and `--limit` page the list |
| `pak search` | Paths whose name contains the word. `--inside` searches file contents. Slower |
| `pak preview` | Print JSON, or metadata when the asset is not JSON |
| `pak open` | Same family as preview |
| `pak snippet` | Print a starter `:row` / `:field` line for [the DSL](../mods/dsl.md) |
| `pak extract` | Write one asset to `-o` |

`--json` on list, search, preview, open, and snippet prints JSON instead of text.

A search that returns nothing is a vocabulary miss, not an empty game. Try a neighboring word (`Stamina`, `Farming`, `Trait`, `Durability`). Drop `--ext json` when you need a `.uasset` curve.

## Other

```powershell
utool check Icarus
utool games list
utool games probe "D:\path\to\Game"
utool --version
```

`utool check` with no id checks the default game in `utool.json`. With a directory, it probes an install that is not configured yet.
