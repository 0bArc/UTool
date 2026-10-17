# Recipes

Each recipe is a full `mod.lua`. Build with `utool mod build <folder> --force-extract`. The file names are real Icarus tables. Confirm the row and the field with `utool pak preview` after a game update before you trust the snippet.

## Set one field

Cave affliction chance to 0.

```lua
utool.mod {
  id = "icarus.nocavesickness",
  name = "No Cave Sickness",
  version = "1.0.0",
  target = { gameId = "Icarus" },
  pak = {
    output = "dist/nocavesickness_P.pak",
    mountPoint = "@auto",
    sourcePak = "@data",
    useUnrealPak = true,
    zip = true,
  },
}

utool.asset("D_AfflictionChance.json")
  :row("Underground_Escalation")
  :field("ChanceInPercent")
  :set(0)
```

Lookup: `utool pak search AfflictionChance --from Icarus --ext json`.

## Multiply one column

Experience from actions, 2x, ratios kept.

```lua
utool.mod {
  id = "icarus.morexp",
  name = "More XP",
  version = "1.0.0",
  target = { gameId = "Icarus" },
  pak = {
    output = "dist/morexp_P.pak",
    mountPoint = "@auto",
    sourcePak = "@data",
    useUnrealPak = true,
    zip = true,
  },
}

utool.asset("D_ExperienceEvents.json"):map("Rows", function(row)
  local xp = row.ExperienceGranted
  if type(xp) == "number" and xp > 0 then
    row.ExperienceGranted = math.floor(xp * 2 + 0.5)
  end
  return row
end)
```

The same shape divides a duration. Processor recipes use `RequiredMillijoules` on `D_ProcessorRecipes.json`. Rows that omit the field inherit the vanilla default (2500 in current Icarus data). Divide that row only, and clamp at 1:

```lua
row.RequiredMillijoules = math.max(1, math.floor(mj / mult + 0.5))
```

## Several zips

2x, 5x, and 10x XP. Details of `:Value` and the loop-variable capture are in [Variants](variants.md).

```lua
utool.mod {
  id = "icarus.morexp",
  name = "More XP",
  version = "1.0.0",
  updateVersion = 1,
  target = { gameId = "Icarus" },
  pak = {
    mountPoint = "@auto",
    sourcePak = "@data",
    useUnrealPak = true,
    output = "dist/morexp_%updateversion%_%d_P.pak",
  },
}

local events = utool.asset("D_ExperienceEvents.json")
local tiers = {
  { mult = 2, tag = "2x", key = 2 },
  { mult = 5, tag = "5x", key = 5 },
  { mult = 10, tag = "10x", key = 10 },
}

for _, tier in ipairs(tiers) do
  local mult = tier.mult
  utool.pak.create(events, function(row)
    local xp = row.ExperienceGranted
    if type(xp) == "number" and xp > 0 then
      row.ExperienceGranted = math.floor(xp * mult + 0.5)
    end
    return row
  end):Value(tier.key)
    :zip("MoreXP-Week-%updateversion%_" .. tier.tag .. ".zip")
end
```

## Delete named rows

```lua
utool.mod {
  id = "icarus.norockgolem",
  name = "No Rock Golem",
  version = "1.0.0",
  target = { gameId = "Icarus" },
  pak = {
    output = "dist/norockgolem_P.pak",
    mountPoint = "@auto",
    sourcePak = "@data",
    useUnrealPak = true,
    zip = true,
  },
}

utool.patch_asset("D_AICreatureType.json", function()
  utool.editor:RemoveArrayElementsWhere("/Rows", "Name", "RockGolem")
end)
```

Spawn, rule, and shop tables use the same call on their own files. A creature that still appears is still named in a second table. Search `RockGolem` across `@data` and delete each list you mean to clear.

## Level cap

JSON cap plus the experience curve. Mount curves and talent curves are the same `patch_curve` with different `AssetName` values. See [Curves](curves.md).

```lua
utool.mod {
  id = "icarus.levelcap",
  name = "Level Cap",
  version = "1.0.0",
  target = { gameId = "Icarus" },
  pak = {
    output = "dist/levelcap_P.pak",
    mountPoint = "@auto",
    sourcePak = "@data",
    curveSourcePak = "@paks",
    useUnrealPak = true,
    zip = true,
  },
}

utool.asset("D_CharacterGrowth.json")
  :row("Player")
  :field("MaxDisplayLevel")
  :set(250)

utool.patch_curve("C_PlayerExperienceGrowth", "Data/Character", function()
  local last = utool.curve:LastKey()
  local xp = last.Value
  for level = math.floor(last.Time) + 1, 250 do
    xp = xp + 144000
    utool.curve:AddKey(level, xp)
  end
end)
```

`144000` is an example step taken from a vanilla key gap. Read `LastKey` and decide the step from the real curve. Do not copy the constant into a different curve.
