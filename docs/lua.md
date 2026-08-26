# Lua mod API

Author mods with `mod.json` `scripts` and `utool.patch_*` callbacks. Scripts register at load; mutation runs during prepare.

## Scripts

List Lua files in `mod.json` under `scripts`. They load in order into one VM.

```json
"scripts": [
  "scripts/extend_progression.lua",
  "scripts/level_display_cap.lua"
]
```

## Registration

```lua
utool.patch_asset(file [, relativeDir], function()
  -- mutate via utool.editor
end)

utool.patch_curve(asset [, relativeDir], function()
  -- mutate via utool.curve
end)
```

`utool.editor` / `utool.curve` exist only inside the matching callback.

### `relativeDir` vs mount

Prepared path is `relativeDir/filename` when `relativeDir` is set; otherwise flat filename.

| Goal | mountPoint | relativeDir |
|---|---|---|
| Content root + nested data | `../../../Icarus/Content/` | `data/Experience` |
| Already under Character | `../../../Icarus/Content/Data/Character/` | omit (flat) |

## `utool.editor` (JSON)

| Method | Role |
|---|---|
| `Get(pointer)` | Read value at JSON pointer |
| `Set(pointer, value)` | Create or overwrite |
| `Add(pointer, value)` | Insert (fails if exists) |
| `Replace(pointer, value)` | Overwrite existing |
| `Append(arrayPointer, value)` | Push onto array |
| `MergeInto(pointer, value)` | Object merge |
| `Remove(pointer)` | Delete |
| `ReplaceAll(name, value [, under])` | Replace all matching property names |
| `MapArray(arrayPointer, fn)` | `fn(row) → row` for each element |
| `SetOnArrayElementsWhere(array, matchProp, matchValue, propPointer, value)` | Constant write where match |
| `RemoveArrayElementsWhere(array, matchProp, matchValue)` | Delete matching elements |

JSON pointers: `/Rows`, `/MaxDisplayLevel`, `/Rows/0/ExperienceGranted`.

## `utool.curve` (CurveFloat)

| Method | Role |
|---|---|
| `AssetName` | Cooked asset base name |
| `LastKey()` | `{ Time, Value }` of last key |
| `AddKey(time, value)` | Append key |
| `SetKey(time, value)` | Set or insert at time |

## Examples

Display cap (flat under Character mount):

```lua
utool.patch_asset("D_CharacterGrowth.json", function()
  utool.editor:SetOnArrayElementsWhere("/Rows", "Name", "Player", "/MaxDisplayLevel", 250)
end)
```

10× XP rows (Content mount + nested path):

```lua
local Multiplier = 10

utool.patch_asset("D_ExperienceEvents.json", "data/Experience", function()
  utool.editor:MapArray("/Rows", function(row)
    local xp = row.ExperienceGranted
    if type(xp) == "number" then
      row.ExperienceGranted = xp * Multiplier
    end
    return row
  end)
end)
```

See [examples/250cap](../examples/250cap) and [examples/morexp](../examples/morexp).
