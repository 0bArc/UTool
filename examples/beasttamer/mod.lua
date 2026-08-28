utool.mod {
  id = "icarus.beastwhisper",
  name = "Beast Whisper",
  version = "1.0.0",
  description = "Tame creatures faster — pick 1.5x through 10x shorter tame timers.",
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
    output = "dist/fastertame%updateversion%_%d_P.pak",
  },
}

local tamesTable = utool.asset("D_Tames.json")

local function scaleDuration(value, mult)
  if type(value) ~= "number" or value <= 0 then
    return value
  end
  return math.max(1, math.floor(value / mult + 0.5))
end

local function scaleTameRow(row, mult)
  row.TameDurationInSeconds = scaleDuration(row.TameDurationInSeconds, mult)
  return row
end

local tiers = {
  { mult = 1.5, tag = "1.5x", key = 15 },
  { mult = 2, tag = "2x", key = 2 },
  { mult = 5, tag = "5x", key = 5 },
  { mult = 10, tag = "10x", key = 10 },
}

for _, tier in ipairs(tiers) do
  local mult = tier.mult
  local tag = tier.tag
  utool.pak.create(tamesTable, function(row)
    return scaleTameRow(row, mult)
  end):Value(tier.key)
    :zip("FasterTame-Week-%updateversion%_" .. tag .. "Faster.zip")
end
