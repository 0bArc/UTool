utool.mod {
  id = "icarus.snapcraft",
  name = "Snap Craft",
  version = "1.0.0",
  description = "All recipes cost no materials and craft in one millijoule (effectively instant).",
  author = "xynv4",
  updateVersion = 247,
  target = {
    gameId = "Icarus",
    engineVersion = "4.27",
    minGameVersion = "1.0.0",
  },
  pak = {
    mountPoint = "@auto",
    sourcePak = "@data",
    useUnrealPak = true,
    output = "dist/snapcraft_%updateversion%_P.pak",
    zip = true,
  },
}

local INSTANT_MILLIJOULES = 1

local function zeroInputCosts(inputs)
  if type(inputs) ~= "table" then
    return
  end
  for _, input in ipairs(inputs) do
    if type(input) == "table" then
      if type(input.Count) == "number" then
        input.Count = 0
      end
      if type(input.RequiredUnits) == "number" then
        input.RequiredUnits = 0
      end
    end
  end
end

local function freeInstantRecipe(row)
  row.RequiredMillijoules = INSTANT_MILLIJOULES
  zeroInputCosts(row.Inputs)
  zeroInputCosts(row.ResourceInputs)
  zeroInputCosts(row.QueryInputs)
  return row
end

utool.patch_asset("D_ProcessorRecipes.json", function()
  utool.editor:Set("/Defaults/RequiredMillijoules", INSTANT_MILLIJOULES)
  utool.editor:Set("/Defaults/Inputs/0/Count", 0)
end)

utool.patch_asset("D_ExtractorRecipes.json", function()
  utool.editor:Set("/Defaults/RequiredMillijoules", INSTANT_MILLIJOULES)
  utool.editor:Set("/Defaults/Inputs/0/Count", 0)
end)

utool.asset("D_ProcessorRecipes.json"):map("Rows", freeInstantRecipe)
utool.asset("D_ExtractorRecipes.json"):map("Rows", freeInstantRecipe)
