local MaxLevel = 250
local XpPerLevel = 144000

local TalentPointsPerLevel = 2
local BlueprintPointsPerLevel = 3
local SoloPointsPerLevel = 1

local function extendXpCurve()
  local curve = utool.curve
  local last = curve:LastKey()
  local xp = last.Value
  for level = math.floor(last.Time) + 1, MaxLevel do
    xp = xp + XpPerLevel
    curve:AddKey(level, xp)
  end
end

local function extendLinearCurve(gainPerLevel)
  local curve = utool.curve
  local last = curve:LastKey()
  local value = last.Value
  for level = math.floor(last.Time) + 1, MaxLevel do
    value = value + gainPerLevel
    curve:AddKey(level, value)
  end
end

local function apply()
  local name = utool.curve.AssetName
  if name == "C_PlayerExperienceGrowth" then
    extendXpCurve()
  elseif name == "C_PlayerTalentGrowth" then
    extendLinearCurve(TalentPointsPerLevel)
  elseif name == "C_PlayerBlueprintGrowth" then
    extendLinearCurve(BlueprintPointsPerLevel)
  elseif name == "C_SoloTalentGrowth" then
    extendLinearCurve(SoloPointsPerLevel)
  end
end

utool.patch_curve("C_PlayerExperienceGrowth", "Data/Character", apply)
utool.patch_curve("C_PlayerTalentGrowth", "Data/Character", apply)
utool.patch_curve("C_PlayerBlueprintGrowth", "Data/Character", apply)
utool.patch_curve("C_SoloTalentGrowth", "Data/Character", apply)
