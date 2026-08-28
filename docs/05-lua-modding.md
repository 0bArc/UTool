# 05 — Lua modding

Author mods in **`mod.lua`**. Legacy `mod.json` still works; if both exist, **`mod.lua` wins**.

## Entry block

```lua
utool.mod {
  id = "icarus.my_mod",
  name = "My Mod",
  version = "1.0.0",
  updateVersion = 247,  -- optional; fills %updateversion% in output templates
  author = "you",
  target = {
    gameId = "Icarus",
    engineVersion = "4.27",
    minGameVersion = "1.0.0",
  },
  scripts = { "scripts/extra.lua" },  -- optional; mod.lua always loads first
  pak = {
    output = "dist/my_mod_P.pak",
    mountPoint = "@auto",
    sourcePak = "@data",
    curveSourcePak = "@paks",  -- when using patch_curve
    useUnrealPak = true,
    zip = true,  -- or zip = "MyMod-Week-%updateversion%.zip"
  },
}
```

## Data DSL

```lua
local field = utool.asset("D_AfflictionChance.json")
  :row("Underground_Escalation")
  :field("ChanceInPercent")

field:set(0)
```

| Call | Meaning |
|------|---------|
| `utool.asset(path)` | JSON table. Bare `D_Foo.json` resolves when unique under `@data` |
| `:row(name)` | Row in `/Rows` where `Name == name` |
| `:find(collection, { Key = value })` | Match rows in an array |
| `:field(prop)` | Field on the current row |
| `:set(value)` / `:set(prop, value)` | Queue a write |
| `:map(collection, fn)` | `fn(row) → row` for each element |

`utool.editor` and `utool.patch_asset` exist for edge cases; prefer the DSL.

## Map entire table with pak.create

For per-variant transforms (see [07 — Multi-variant mods](07-multi-variant-mods.md)):

```lua
local decayTable = utool.asset("D_Decayable.json")

utool.pak.create(decayTable, function(row)
  row.SpoilTime = math.floor(row.SpoilTime * 2)
  return row
end):Value(2)
```

## Curves

```lua
utool.patch_curve("C_PlayerExperienceGrowth", "Data/Character", function()
  local last = utool.curve:LastKey()
  utool.curve:AddKey(last.Time + 1, last.Value + 144000)
end)
```

Inside `patch_curve` only: `AssetName`, `LastKey()`, `AddKey(time, value)`, `SetKey(time, value)`.

## Mount point

`mountPoint = "@auto"` resolves at build from `utool.json` and `paksDir`. It is not baked into the pak.

| Goal | mountPoint | Asset path |
|------|------------|------------|
| Standard Content override | `@auto` | bare `D_….json` or `data/…/D_….json` |
| Character-only mount | explicit `…/Content/Data/Character/` | bare `D_CharacterGrowth.json` |

## Build pipeline (what happens)

```text
mod.lua (+ scripts)
  → extract source assets from @data / @paks
  → apply patches / maps / curves
  → write .cache/prepared
  → UnrealPak → dist/*_P.pak
  → optional zip (pak removed after zip)
```

## Next

[06 — Build and deploy](06-build-and-deploy.md)
