utool.mod {
  id = "icarus.level250cap",
  name = "Level 250 Cap",
  version = "1.1.0",
  description = "Player level cap 250 + mount/tame cap 100, with XP/talent curves extended for both.",
  author = "utool",
  target = {
    gameId = "Icarus",
    engineVersion = "4.27",
    minGameVersion = "1.0.0",
  },
  scripts = {
    "scripts/extend_progression.lua",
  },
  pak = {
    output = "dist/level250cap_P.pak",
    mountPoint = "../../../Icarus/Content/Data/Character/",
    sourcePak = "@data",
    curveSourcePak = "@paks",
    useUnrealPak = true,
    zip = true,
  },
}

-- Visible player cap. Vanilla MaxLevel stays 1000 so orbital XP does not hard-stop at 250.
utool.asset("D_CharacterGrowth.json")
  :row("Player")
  :field("MaxDisplayLevel")
  :set(250)

-- Mounts inherit Defaults MaxLevel 50; raise so extended mount curves apply.
utool.asset("D_CharacterGrowth.json")
  :row("AI_Mounts")
  :field("MaxDisplayLevel")
  :set(100)

utool.asset("D_CharacterGrowth.json")
  :row("AI_Mounts")
  :field("MaxLevel")
  :set(100)
