# Variants

One `mod.lua` can emit several paks. Each one is a variant. Players install one zip. A variant is either a mapped asset with a captured tier, or a single field set to the variant's number.

## Placeholders

| Token | Replaced with |
|-------|----------------|
| `%updateversion%` | `updateVersion` from the manifest, `utool.json`, or `--update-version` |
| `%d` | The integer from `:Value` |
| `%percent%` | That integer plus a `%` sign, so `15` becomes `15%` |
| `%other%` | That integer as text. The name is anything except `updateversion` |

`%d` and `%name%` are only legal on an output that went through `:Value`. A plain one-pak mod that puts `%d` in `pak.output` throws.

`:Value` numbers:

- An integer is stored as that integer. `%d` is that integer. The field value, if this is a field variant, is that integer.
- A float such as `1.5` is stored in the field as `1.5`. `%d` becomes `15` (`value * 10`, truncated toward zero). Use an integer `key` in the filename when you want `2` for a 2x tier and a separate tag string for the zip name.

## Map every row, once per tier

`utool.pak.create(asset, function(row) ... end)` maps `/Rows`. The function return value replaces the row, same as `:map`.

`:Value(n)` commits one variant. `:zip(template)` names that variant's zip. Call `:Value` before `:zip` so the zip is attached to the variant you just committed.

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

The `local mult = tier.mult` is required. The callback runs later. The loop variable `tier` would otherwise be the last row for every callback.

`:also(otherAsset)` adds another asset to the same map. `utool.pak.create({ assetA, "D_Other.json" }, fn)` does the same with a table of handles or path strings. An empty table throws `utool.pak.create({assets}, fn) needs at least one asset`. A non-string, non-handle entry throws `expects AssetRef or path strings`.

## One field, many values

```lua
local chance = utool.asset("D_AfflictionChance.json")
  :row("Underground_Escalation")
  :field("ChanceInPercent")

for n = 0, 20, 5 do
  utool.pak.create(chance):Value(n)
    :zip("NoCaveSickness-Week-%updateversion%_%dChance.zip")
end
```

Here `:Value` is the number written into the field and the number in `%d`. `pak.output` still needs `%d` if each tier is its own pak file.

`:Value` on a builder that has neither a map function nor a field throws `pak.create(...):Value called on invalid builder`. `:Value` of a non-number throws `pak.create(...):Value expects a number`.

## zip

`:zip()` with no argument marks the last variant to zip with the default name. `:zip("Name.zip")` sets the template. `pak.zip = true` or `pak.zip = "Name.zip"` is the single-pak form, when you are not using `:Value`.

One `utool mod build` runs every variant. The console prints one pak line and, when zipped, one zip line per tier. Ship the zips. Installing two tiers means two copies of the same asset path. The game does not merge them in your favor.
