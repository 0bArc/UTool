utool.mod {
  id = "icarus.freshrations",
  name = "Fresh Rations",
  version = "1.0.0",
  description = "Food and volatiles spoil slower — pick 1.5x through 25x decay timers.",
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
    output = "dist/freshrations_%updateversion%_%d_P.pak",
  },
}

local decayTable = utool.asset("D_Decayable.json")

local skipRows = {
  Decay_NoDecay = true,
  Decay_Quick = true,
}

local function scaleTime(value, mult)
  if type(value) ~= "number" or value <= 0 then
    return value
  end
  return math.floor(value * mult + 0.5)
end

local function scaleDecayRow(row, mult)
  local name = row.Name
  if type(name) == "string" and skipRows[name] then
    return row
  end
  row.DecayTime = scaleTime(row.DecayTime, mult)
  row.SpoilTime = scaleTime(row.SpoilTime, mult)
  return row
end

local tiers = {
  { mult = 1.5, tag = "1.5x", key = 15 },
  { mult = 2, tag = "2x", key = 2 },
  { mult = 5, tag = "5x", key = 5 },
  { mult = 10, tag = "10x", key = 10 },
  { mult = 15, tag = "15x", key = 150 },
  { mult = 20, tag = "20x", key = 20 },
  { mult = 25, tag = "25x", key = 25 },
}

for _, tier in ipairs(tiers) do
  local mult = tier.mult
  local tag = tier.tag
  utool.pak.create(decayTable, function(row)
    return scaleDecayRow(row, mult)
  end):Value(tier.key)
    :zip("FreshRations-Week-%updateversion%_" .. tag .. "Slower.zip")
end
