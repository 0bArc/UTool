# Curves

Some numbers are not in a datatable. Icarus player level is a field on `D_CharacterGrowth.json` and a `CurveFloat` `.uasset` (`C_PlayerExperienceGrowth` and the talent, blueprint, and solo curves). Raising only the JSON field leaves the curve short. The game stops where the keys stop.

Curves are read from `curveSourcePak` (usually `@paks`, not `@data`). Set that on `pak` in the [manifest](manifest.md).

```lua
utool.patch_curve("C_PlayerExperienceGrowth", "Data/Character", function()
  local last = utool.curve:LastKey()
  local xp = last.Value
  for level = math.floor(last.Time) + 1, 250 do
    xp = xp + 144000
    utool.curve:AddKey(level, xp)
  end
end)
```

`utool.patch_curve(assetName [, relativeDir], function)`. The relative directory defaults to nothing unless you pass it. Icarus character curves live under `Data/Character`. Wrong arity throws `utool.patch_curve(assetName [, relativeDir], function)`. A throw inside the function fails the build with `Lua curve patch failed:`.

Inside the function, `utool.curve` is set. Outside it, `utool.curve` is nil.

## Curve editor

| Call | Effect |
|------|--------|
| `utool.curve.AssetName` | The asset name passed to `patch_curve` |
| `LastKey()` | The key with the greatest `Time`. Throws `Curve '...' has no keys.` when the file had none |
| `AddKey(time, value)` | Insert a key. If a key already sits within `0.0001` of `time`, that key's value is replaced |
| `SetKey(time, value)` | Same function as `AddKey` |
| `ScaleValues(factor)` | Multiply every existing key's value by `factor` |

Keys are `{ Time = number, Value = number }`. The same fields are also exposed as `time` and `value`.

`LastKey` does not remove keys. `AddKey` replaces a key whose time is already within `0.0001`, otherwise it appends. Extending a curve means "from the last vanilla time, add one key per new level".

`ScaleValues` runs on the keys loaded so far, including keys you already added in this function. Call it before `AddKey` if you only meant to scale the vanilla curve.

## One function, several curves

`AssetName` is how a shared function tells the curves apart:

```lua
local function apply()
  local name = utool.curve.AssetName
  if name == "C_PlayerExperienceGrowth" then
    local last = utool.curve:LastKey()
    local xp = last.Value
    for level = math.floor(last.Time) + 1, 250 do
      xp = xp + 144000
      utool.curve:AddKey(level, xp)
    end
  end
end

utool.patch_curve("C_PlayerExperienceGrowth", "Data/Character", apply)
utool.patch_curve("C_PlayerTalentGrowth", "Data/Character", apply)
```

Each `patch_curve` loads that asset's own keys and then calls `apply`.

## Cooked data

The writer rebuilds the RichCurve key array in the `.uasset` / `.uexp` when it recognizes the cooked layout. A large edit the game then rejects is usually a curve payload it will not deserialize, not the JSON row. Keep the new keys in order, and do not leave a gap the game treats as the end of the curve. There is no separate "compile curve" command. The build is the compile.

Search before you name the asset:

```powershell
utool pak search PlayerExperienceGrowth --from Icarus
```

Drop `--ext json` for this search. Curves are `.uasset`, not datatable JSON.
