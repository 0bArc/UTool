local MaxLevel = 250
local MountMaxLevel = 100
local XpPerLevel = 144000

local TalentPointsPerLevel = 2
local BlueprintPointsPerLevel = 3
local SoloPointsPerLevel = 1

local function extendXpCurve()
  local last = utool.curve:LastKey()
  local xp = last.Value
  for level = math.floor(last.Time) + 1, MaxLevel do
    xp = xp + XpPerLevel
    utool.curve:AddKey(level, xp)
  end
end

local function extendLinearCurve(gainPerLevel)
  local last = utool.curve:LastKey()
  local value = last.Value
  for level = math.floor(last.Time) + 1, MaxLevel do
    value = value + gainPerLevel
    utool.curve:AddKey(level, value)
  end
end

local function extendMountFromLast()
  local last = utool.curve:LastKey()
  local time = math.floor(last.Time)
  local per = 1
  if time > 0 then
    per = last.Value / time
  end
  local value = last.Value
  for level = time + 1, MountMaxLevel do
    value = value + per
    utool.curve:AddKey(level, value)
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
  elseif name == "C_MountExperienceGrowth" or name == "C_MountTalentGrowth" then
    extendMountFromLast()
  end
end

utool.patch_curve("C_PlayerExperienceGrowth", "Data/Character", apply)
utool.patch_curve("C_PlayerTalentGrowth", "Data/Character", apply)
utool.patch_curve("C_PlayerBlueprintGrowth", "Data/Character", apply)
utool.patch_curve("C_SoloTalentGrowth", "Data/Character", apply)
utool.patch_curve("C_MountExperienceGrowth", "Data/Character", apply)
utool.patch_curve("C_MountTalentGrowth", "Data/Character", apply)
