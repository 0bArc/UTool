utool.mod {
  id = "icarus.noplantfatigue",
  name = "No plant fatigue",
  version = "1.1.1",
  description = "Removes plant fatigue: no stack gain, no Seed_Fatigue modifier, no plant-fatigue icon.",
  author = "utool",
  target = {
    gameId = "Icarus",
    engineVersion = "4.27",
    minGameVersion = "1.0.0",
  },
  pak = {
    output = "dist/noplantfatigue_P.pak",
    mountPoint = "@auto",
    sourcePak = "@data",
    useUnrealPak = true,
    zip = true,
  },
}

-- Defaults apply to every seed that does not override FatigueIncreaseEachHarvest.
utool.patch_asset("D_FarmingSeeds.json", function()
  utool.editor:Set("/Defaults/FatigueIncreaseEachHarvest", 0)
  utool.editor:Set("/Defaults/FatigueModifier/RowName", "None")
end)

utool.asset("D_FarmingSeeds.json"):map("Rows", function(row)
  if type(row.FatigueIncreaseEachHarvest) == "number" then
    row.FatigueIncreaseEachHarvest = 0
  end
  if type(row.FatigueModifier) == "table" then
    row.FatigueModifier.RowName = "None"
  end
  return row
end)

-- Debuff definition (icon + stats) if anything still applies Seed_Fatigue.
utool.asset("D_ModifierStates.json")
  :row("Seed_Fatigue")
  :field("ModifierIcon")
  :set("")

utool.asset("D_ModifierStates.json")
  :row("Seed_Fatigue")
  :field("GrantedStats")
  :set({
    ['(Value="BasePlantedCropGrowthSpeed_+%")'] = 0,
    ['(Value="BasePlantedCropYield_+%")'] = 0,
  })
