# Editor

`utool.patch_asset` opens one JSON file and runs a function with `utool.editor` set. When the function returns, `utool.editor` is cleared. Use this for deletes, deep pointers, and bulk renames. Prefer the [Data DSL](dsl.md) for a single field.

```lua
utool.patch_asset("D_AICreatureType.json", function()
  utool.editor:RemoveArrayElementsWhere("/Rows", "Name", "RockGolem")
end)

utool.patch_asset("D_Foo.json", "Data/AI", function()
  utool.editor:Set("/Rows/0/SomeField", 1)
end)
```

The optional middle string is the relative directory inside the pak, same idea as the second argument of `patch_curve`. Wrong arity throws `utool.patch_asset(file [, relativeDir], function)`. A throw inside the function fails the build with `Lua asset patch failed:`.

Pointers are JSON Pointers. `/Rows` is the datatable array. `/Rows/0/Name` is the first element's `Name`. Indexes are zero-based in the pointer and one-based in Lua tables. Prefer matching on `Name` so a game update that reorders rows does not silently edit the wrong one.

## Methods

`Get` returns a Lua value. `toJson` is not exposed. You edit, you do not pretty-print from the script.

| Call | Effect |
|------|--------|
| `Get(pointer)` | Read. Missing path throws `Missing path: ...` |
| `Set(pointer, value)` | Write. Creates missing objects along the path |
| `Replace(pointer, value)` | Overwrite a key or array slot that already exists. Missing key throws `Cannot replace missing path`. Does not create parents |
| `Add(pointer, value)` | On an object, create the key or replace its value. On an array, insert at the index and shift the tail. `/-` appends. Does not create missing parents |
| `Append(pointer, value)` | Push onto an array |
| `MergeInto(pointer, object)` | Deep-merge an object into the object at that pointer. A non-object value throws `Merge value must be a JSON object` |
| `Remove(pointer)` | Delete that key or array slot |
| `ReplaceAll(name, value)` | Every object in the tree that has property `name` gets `value` |
| `ReplaceAll(name, value, underPointer)` | Same, limited to the subtree. Unknown subtree throws `Subtree not found:` |
| `MapArray(pointer, fn)` | Replace each element with `fn(element)`. The return value is the new element. Throws `MapArray callback failed:` |
| `SetOnArrayElementsWhere(arrayPointer, matchProp, matchValue, propPointer, value)` | On each object where `matchProp == matchValue`, set `propPointer`. Returns how many rows changed |
| `RemoveArrayElementsWhere(arrayPointer, matchProp, matchValue)` | Delete those objects. Returns how many were removed |

`SetOnArrayElementsWhere` and the DSL `:row` / `:set` are the same operation. The DSL builds the pointer `/Rows`, the match property `Name`, and the field name for you.

`RemoveArrayElementsWhere` compares the property with JSON equality. `"RockGolem"` does not match `"rockgolem"`. Call it once per name, or call it in a loop. It returns a count. A count of 0 is not an error. Preview the file when you expected a delete.

```lua
local names = { "RockGolem", "Juvenile_RockGolem" }
utool.patch_asset("D_AICreatureType.json", function()
  for _, name in ipairs(names) do
    utool.editor:RemoveArrayElementsWhere("/Rows", "Name", name)
  end
end)
```

`MapArray` can drop or rewrite elements the DSL `:map` can also rewrite. Use `MapArray` when you are already inside `patch_asset` and you need `Get` on a sibling pointer in the same function. Do not queue a DSL `:map` and a `MapArray` on the same array and expect a defined merge. Both run as separate prepare steps on the extracted file. Later steps see earlier writes. Keep one writer per array unless you have read the prepare order for that asset and you mean the stacking.

## When the game ignores a successful build

The pak mounted, and the JSON path inside it does not match what the game opens. `mountPoint` and the file's directory have to agree. A bare `D_Foo.json` belongs at a mount that already ends at that file's directory. A path `Data/AI/D_Foo.json` belongs at a mount that ends at `Content/`. [Manifest](manifest.md) describes `@auto`.
