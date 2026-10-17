# Data DSL

Use this when the change is "this row, this field" or "every row in this array". You do not write JSON pointers. The editor page is for deletions and for shapes this DSL cannot say.

Paths are file names or pak-relative paths. A bare `D_ExperienceEvents.json` is enough when that file name is unique under the source pak. If it is not unique, pass a longer path (`Data/Experience/D_ExperienceEvents.json`). The file has to be found by `utool pak search` first. Invented names fail at extract time.

## utool.asset

```lua
local events = utool.asset("D_ExperienceEvents.json")
```

Returns an asset handle. An empty file name throws `utool.asset requires a file name`.

## :row and :field and :set

```lua
utool.asset("D_AfflictionChance.json")
  :row("Underground_Escalation")
  :field("ChanceInPercent")
  :set(0)
```

`:row(name)` means the `/Rows` element whose `Name` equals `name`.

`:field(prop)` selects that property on the current row.

`:set(value)` queues a write of the current field. On a row, `:set(prop, value)` queues a write of that property without a separate `:field` call.

Values are Lua nil, bool, number, string, or table. Tables follow the [JSON rules](json.md).

The write is a match, not an index. Every `/Rows` element with that `Name` is updated. Zero matches means the prepared file is unchanged. Check the name in `utool pak preview` when a build succeeds and the game does not change.

## :find

`:row` is the common case of `/Rows` plus `Name`. `:find` is the general form.

```lua
utool.asset("D_Itemable.json")
  :find("Rows", { Name = "Iron_Ingot" })
  :field("MaxStack")
  :set(500)
```

The table is AND. Every pair must match. Several keys are allowed. The collection name is the array (`"Rows"` becomes the pointer `/Rows`).

## :map

```lua
utool.asset("D_ExperienceEvents.json"):map("Rows", function(row)
  local xp = row.ExperienceGranted
  if type(xp) == "number" and xp > 0 then
    row.ExperienceGranted = math.floor(xp * 2 + 0.5)
  end
  return row
end)
```

`:map(collection, fn)` runs `fn` on every element of that array. The return value replaces the element. Return the same table after editing it. Returning nil drops the JSON value to null.

The callback runs during prepare, not while `mod.lua` is still being read. A runtime error fails the build with `asset:map callback failed:`.

Guard types. Datatables mix numbers, missing keys, and the occasional non-number. `row.ExperienceGranted * 2` throws when the field is absent.

`:map` is one result for the whole asset. It does not by itself make 2x and 5x zips. That is [Variants](variants.md).

## What this DSL will not do

- Delete a row. Use [Editor](editor.md) `RemoveArrayElementsWhere`.
- Walk a nested pointer such as `/Rows/0/Stats/0`. Use the editor, or edit the nested table inside `:map` (`row.Stats` is a Lua table when the JSON value was an array or object).
- Read the file back. There is no `:get` on the DSL. Use `utool pak preview` while you are writing the mod, or `utool.editor:Get` inside `patch_asset`.

## Find the names

```powershell
utool pak search Experience --from Icarus --ext json
utool pak preview Data/Experience/D_ExperienceEvents.json --from @data
utool pak snippet Data/Experience/D_ExperienceEvents.json --from @data --row ChopTree --field ExperienceGranted
```

`pak snippet` prints a starter `:row` / `:field` line. Paste it, then change the value. The row and field strings are case-sensitive and must match the file.
