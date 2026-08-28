# 07 — Multi-variant mods

One `mod.lua`, many downloads — 2x / 5x / 10x tiers, affliction percentages, etc.

## Pattern

```lua
utool.mod {
  id = "icarus.freshrations",
  name = "Fresh Rations",
  updateVersion = 247,
  pak = {
    mountPoint = "@auto",
    sourcePak = "@data",
    useUnrealPak = true,
    output = "dist/freshrations_%updateversion%_%d_P.pak",
  },
}

local decayTable = utool.asset("D_Decayable.json")

local tiers = {
  { mult = 1.5, tag = "1.5x", key = 15 },
  { mult = 2, tag = "2x", key = 2 },
  { mult = 5, tag = "5x", key = 5 },
  { mult = 10, tag = "10x", key = 10 },
}

for _, tier in ipairs(tiers) do
  local mult = tier.mult
  utool.pak.create(decayTable, function(row)
    row.SpoilTime = math.floor(row.SpoilTime * mult)
    return row
  end):Value(tier.key)
    :zip("FreshRations-Week-%updateversion%_" .. tier.tag .. "Slower.zip")
end
```

## Build command

Same as any mod — one run builds all variants:

```powershell
utool pak build-mod examples/freshrations --force-extract
```

Console shows one `Built mod pak` + `Zipped:` line per tier.

## `:Value(n)`

Registers one output variant. `%d` in `pak.output` becomes that number.

| Token | Source |
|-------|--------|
| `%updateversion%` | `updateVersion` in `utool.mod` |
| `%d` | integer passed to `:Value(...)` |

Use `key = 15` for a 1.5x tier when `%d` must stay an integer in the pak filename.

## `:zip(template)`

Creates a zip next to (or under) `dist/`, then deletes the pak.

```lua
:zip("BeastWhisper-Week-%updateversion%_" .. tag .. "Faster.zip")
```

Or enable zip for a single-pak mod in the manifest:

```lua
pak = {
  output = "dist/level250cap_P.pak",
  zip = true,
}
```

## Single-field variants

When every tier changes one field on one row:

```lua
local chanceField = utool.asset("D_AfflictionChance.json")
  :row("Underground_Escalation")
  :field("ChanceInPercent")

for chance = 0, 25, 5 do
  utool.pak.create(chanceField):Value(chance)
    :zip("NoCaveSickness-Week-%updateversion%_%d%Chance.zip")
end
```

## Examples

| Mod | Variants |
|-----|----------|
| `examples/affliction_chance` | 0–25% affliction |
| `examples/freshrations` | 1.5x–25x slower spoil |
| `examples/beasttamer` | 1.5x–10x faster tame |

## Next

[08 — Pak Studio](08-pak-studio.md)
