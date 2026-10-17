# JSON rules

Lua tables do not have a separate array type. UTool has to decide whether a table becomes a JSON array `[]` or a JSON object `{}`. Getting this wrong writes a file the game will not load. The usual break is an empty `Outputs` or `Inputs` array saved as `{}`.

## How a table is classified

When a Lua table is written back to JSON:

1. If it has no keys and it was produced by reading a JSON array, it stays `[]`. Those tables carry an internal mark (`__utool_json_array` on the metatable).
2. If it has no keys and you wrote `{}` yourself, it becomes `{}`.
3. If every key is an integer from 1 through N with no holes, it becomes an array, in order.
4. Otherwise it becomes an object. Integer keys are stringified (`1` becomes `"1"`).

So:

```lua
row.Outputs = {}           -- object {}, even if Outputs was an array
row.Outputs = row.Outputs  -- still an array if you never replaced it
```

Do not assign a fresh `{}` over an array to "clear" it. Remove elements with `RemoveArrayElementsWhere`, or inside `:map` remove keys from the existing table and leave the table the asset gave you. An empty marked table round-trips as `[]`.

Holes break the array test. `{ [1] = "a", [3] = "c" }` is an object, not a sparse array. Build dense 1-based arrays.

## Scalars

| Lua | JSON |
|-----|------|
| `nil` | `null` |
| `true` / `false` | `true` / `false` |
| integer number | integer |
| other number | float |
| string | string |

Integers that were integers in the file come back as integers. Do not assume every number is a float. `math.floor(x * mult + 0.5)` is the right way to scale a count. Dividing a duration should keep a floor of `1` when `0` means "unset" or "instant" in that table and you did not mean instant.

## Reading inside :map

`row.Field` is `nil` when the key is missing or the JSON value is `null`. Check `type(row.Field) == "number"` before arithmetic. Nested objects are tables. Nested arrays are tables and, when they came from JSON, they keep the array mark even if empty.

The mark is not part of the public API. Do not set `__utool_json_array` yourself. Return the tables the loader gave you.
