# Manifest

`utool.mod` takes one table. It records the mod. It does not patch anything by itself. Call it before the patches. The patches can also sit above it. Order between `utool.mod` and the patch calls does not matter, because patches are queued and applied after the script returns. Calling `utool.mod` twice throws.

```lua
utool.mod {
  id = "icarus.morexp",
  name = "More XP",
  version = "1.0.0",
  description = "Doubles ExperienceGranted.",
  author = "you",
  updateVersion = 1,
  target = {
    gameId = "Icarus",
    engineVersion = "4.27",
    minGameVersion = "1.0.0",
    maxGameVersion = "9.9.9",
  },
  scripts = { "scripts/extra.lua" },
  contentRoots = { "content" },
  pak = {
    output = "dist/morexp_%updateversion%_P.pak",
    mountPoint = "@auto",
    sourcePak = "@data",
    curveSourcePak = "@paks",
    sourceFilter = nil,
    useUnrealPak = true,
    keepCache = false,
    zip = "MoreXP-Week-%updateversion%.zip",
  },
}
```

## Required

| Field | Rule |
|-------|------|
| `id` | Non-empty string. Conventional shape is `game.shortname`, for example `icarus.morexp`. This is the mod id, not the Nexus page id |
| `name` | Non-empty display name |

Missing either one throws `utool.mod requires id and name`.

## Optional identity

| Field | Rule |
|-------|------|
| `version` | String. Default `1.0.0` when omitted |
| `description` | String or omit |
| `author` | String or omit |
| `updateVersion` | String or number. Becomes the text in `%updateversion%`. A number is formatted as an integer. When the field is omitted, `utool.json` `updateVersion` is used. `utool mod ship <week>` and `--update-version` override it for that run |

## target

All four keys are optional strings. `gameId` selects the `games` entry in `utool.json` (`Icarus`). `engineVersion`, `minGameVersion`, and `maxGameVersion` are recorded on the manifest. They do not by themselves refuse to build.

The key is `minGameVersion`. A misspelling is ignored, not remapped.

## scripts

Array of paths relative to the mod folder. Each file runs after `mod.lua`. Listing `mod.lua` again is skipped. See [How a mod runs](README.md) for the case with no `mod.lua`.

## contentRoots

Array of directories of loose files to mount into the pak. Default is `{ "content" }` when you omit the key. If you pass a table and it contains no strings, the default `content` is used.

## pak

Omit `pak` and the manifest keeps the C++ defaults (`useUnrealPak` false, no zip). Passing a `pak` table turns `useUnrealPak` on unless you set it to false.

| Field | Rule |
|-------|------|
| `output` | Pak path relative to the mod folder. Supports `%updateversion%`, `%d`, and `%name%` placeholders. See [Variants](variants.md) |
| `mountPoint` | `@auto`, or a full mount string such as `../../../Icarus/Content/` |
| `sourcePak` | Where JSON assets are extracted. `@data` is the usual value |
| `curveSourcePak` | Where curve `.uasset` files are extracted. `@paks` when the curve is not inside `data.pak` |
| `sourceFilter` | Optional filter string passed through to the extract |
| `useUnrealPak` | Bool. Default true when the `pak` table exists |
| `keepCache` | Bool. Default false. True leaves the prepare cache after a successful pack |
| `zip` | `true` zips the pak with a default name. A string zips to that template. Omit or `false` leaves the pak |

`mountPoint = "@auto"` is resolved from `utool.json` before UnrealPak runs. A character-only mount is a literal path ending in the directory that should see a bare `D_CharacterGrowth.json`. If the mount is too high or too low, the game ignores the file and the pak still "builds".

A template that contains `%updateversion%` throws at expand time when no update version is set anywhere. A template that contains `%d` or `%something%` throws when that output was not produced by `:Value`. The error is `template uses %d but no pak.create(...):Value was provided`.
