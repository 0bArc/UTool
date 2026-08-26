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
