utool.mod {
  id = "icarus.nolicenserequirements",
  name = "No license requirements",
  version = "1.0.1",
  description = "Zeros Legendary Licence shop costs and clears RequiredAccountFlag gates on Living Item shop rows.",
  author = "utool",
  target = {
    gameId = "Icarus",
    engineVersion = "4.27",
    minGameVersion = "1.0.0",
  },
  pak = {
    output = "dist/nolicenserequirements_P.pak",
    mountPoint = "@auto",
    sourcePak = "@data",
    useUnrealPak = true,
    zip = true,
  },
}

-- Biolab / Great Hunt living-item shop: Licence currency + account-flag unlocks.
utool.asset("D_LivingItemShopItems.json"):map("Rows", function(row)
  if type(row.Cost) == "table" then
    for _, entry in ipairs(row.Cost) do
      local meta = entry and entry.Meta
      local name = meta and meta.RowName
      if name == "Licence" or name == "License" then
        entry.Amount = 0
      end
    end
  end

  if type(row.RequiredAccountFlag) == "table" then
    row.RequiredAccountFlag.RowName = "None"
  end

  return row
end)
