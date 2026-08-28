utool.mod {
  id = "icarus.morexp",
  name = "More XP (10x)",
  version = "1.1.0",
  description = "10x player ExperienceGranted and 10x mount leveling (mount XP curve ÷10).",
  author = "utool",
  target = {
    gameId = "Icarus",
    engineVersion = "4.27",
    minGameVersion = "1.0.0",
  },
  pak = {
    output = "dist/morexp_P.pak",
    mountPoint = "@auto",
    sourcePak = "@data",
    curveSourcePak = "@paks",
    useUnrealPak = true,
    zip = true,
  },
}

local Multiplier = 3400

utool.asset("D_ExperienceEvents.json"):map("Rows", function(row)
  local xp = row.ExperienceGranted
  if type(xp) == "number" then
    row.ExperienceGranted = xp * Multiplier
  end
  return row
end)

-- Mounts need less XP per level ⇒ same feed/activity XP goes ~10x farther.
utool.patch_curve("C_MountExperienceGrowth", "Data/Character", function()
  utool.curve:ScaleValues(1 / Multiplier)
end)
