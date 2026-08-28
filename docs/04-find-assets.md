# 04 — Find assets to edit

UTool does not invent balance fields. You search the game’s tables, then patch what you find.

## CLI: list and search

```powershell
utool pak list @paks --ext json --limit 50
utool pak search CharacterGrowth --from Icarus --ext json
utool pak preview Data/Character/D_CharacterGrowth.json --from @data --json
utool pak snippet Data/Character/D_CharacterGrowth.json --from @data --row Player --field MaxDisplayLevel
```

| Command | Use |
|---------|-----|
| `pak list` | Browse pak contents |
| `pak search` | Find paths by keyword |
| `pak preview` | Read JSON (or metadata) |
| `pak snippet` | Starter DSL for a row/field |
| `pak extract` | Write one asset to disk |

## Pak Studio (GUI)

1. Open Pak Studio (`tools/PakStudio`, or `npm run studio` from that folder).
2. Load game from `utool.json` or probe an install path.
3. Open or create a project with `mod.lua`.
4. **Assets** panel → search (`health`, `xp`, `tame`, `decay`, …).
5. Click a JSON file → **Preview**.
6. **Insert snippet** → edits land in `mod.lua`.
7. Save → **Build**.

See [08 — Pak Studio](08-pak-studio.md) for UI details.

## Search tips

If nothing matches, try related words: `Stamina`, `Vital`, `Damage`, `Farming`, `Trait`.

Switch browse mode to **All** when JSON filter hides uassets you need for curves.

## Patch patterns

Single row:

```lua
utool.asset("D_CharacterGrowth.json")
  :row("Player")
  :field("MaxDisplayLevel")
  :set(250)
```

Every row in a collection:

```lua
utool.asset("D_ExperienceEvents.json"):map("Rows", function(row)
  if type(row.ExperienceGranted) == "number" then
    row.ExperienceGranted = row.ExperienceGranted * 10
  end
  return row
end)
```

Copy asset names, row `Name`, and field names from Preview — do not guess.

## Example mods by topic

| Goal | Example | Table |
|------|---------|--------|
| More XP | `examples/morexp` | `D_ExperienceEvents.json` |
| Level + mount cap | `examples/250cap` | `D_CharacterGrowth.json` + player/mount curves |
| Affliction chance | `examples/affliction_chance` | `D_AfflictionChance.json` |
| No plant fatigue | `examples/noplantfatigue` | `D_FarmingSeeds.json` |
| Fast crops (dev) | `examples/plantgrowthdev` | `D_FarmingGrowthStates.json` |
| Slower food spoil | `examples/freshrations` | `D_Decayable.json` |
| Faster taming | `examples/beasttamer` | `D_Tames.json` |
| Free instant craft | `examples/snapcraft` | `D_ProcessorRecipes.json`, `D_ExtractorRecipes.json` |
| No licence gates | `examples/nolicenserequirements` | `D_LivingItemShopItems.json` |

## Next

[05 — Lua modding](05-lua-modding.md)
