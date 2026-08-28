utool.mod {
  id = "icarus.plantgrowthdev",
  name = "Plant Growth Dev",
  version = "1.0.0",
  description = "Dev mod: crop stages advance in ~1s; mature crops stay harvestable for 60s before decay.",
  author = "utool",
  target = {
    gameId = "Icarus",
    engineVersion = "4.27",
    minGameVersion = "1.0.0",
  },
  pak = {
    output = "dist/plantgrowthdev_P.pak",
    mountPoint = "@auto",
    sourcePak = "@data",
    useUnrealPak = true,
    zip = true,
  },
}

local StageSeconds = 1
local MatureSeconds = 60

-- Each row is one visual stage; TimeToNextState is seconds until the next stage (or decay from mature).
utool.asset("D_FarmingGrowthStates.json"):map("Rows", function(row)
  local t = row.TimeToNextState
  if type(t) ~= "number" or t <= 1 then
    return row
  end

  local name = row.Name or ""
  if name:match("_05$") then
    row.TimeToNextState = MatureSeconds
  else
    row.TimeToNextState = StageSeconds
  end
  return row
end)
