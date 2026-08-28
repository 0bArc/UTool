utool.mod {
  id = "icarus.affliction_chance_pack",
  name = "Affliction Chance Pack",
  version = "1.0.1",
  description = "Builds one pak per Underground_Escalation chance (0..25 step 5).",
  author = "utool",
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
    output = "dist/week%updateversion%_%d_affliction_chance_P.pak",
  },
}

local chanceField = utool.asset("D_AfflictionChance.json")
  :row("Underground_Escalation")
  :field("ChanceInPercent")



for chance = 0, 25, 5 do
  utool.pak.create(chanceField):Value(chance)
    :zip("NoCaveSickness-Week-%updateversion%_%d%Chance.zip")
end
