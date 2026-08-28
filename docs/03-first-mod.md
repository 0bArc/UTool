# 03 — First mod

Build one example mod end-to-end.

## Pick an example

```powershell
utool discover examples
utool validate examples/morexp
```

Good first builds:

- `examples/morexp` — one JSON table, `:map` multiplier
- `examples/example-mod` — empty stub you fill in

## Build

```powershell
utool pak build-mod examples/morexp --force-extract
```

`--force-extract` re-reads game JSON from `data.pak`. Use after a game update.

Output lands under the mod’s `dist/` folder (see `pak.output` in `mod.lua`).

## Install in game

Copy the `*_P.pak` (or unzip if the mod ships a `.zip`) into:

```text
<Icarus>/Icarus/Content/Paks/mods/
```

Or use deploy (copies the last built pak):

```powershell
utool deploy examples/morexp
```

Launch the game and confirm the change (e.g. more XP from chopping).

## Minimal mod.lua shape

```lua
utool.mod {
  id = "icarus.example",
  name = "Example",
  version = "1.0.0",
  target = { gameId = "Icarus" },
  pak = {
    output = "dist/example_P.pak",
    mountPoint = "@auto",
    sourcePak = "@data",
    useUnrealPak = true,
    zip = true,
  },
}

utool.asset("D_ExperienceEvents.json"):map("Rows", function(row)
  if type(row.ExperienceGranted) == "number" then
    row.ExperienceGranted = row.ExperienceGranted * 10
  end
  return row
end)
```

## Next

[04 — Find assets to edit](04-find-assets.md)
