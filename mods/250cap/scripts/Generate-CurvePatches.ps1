param(
    [int]$MaxLevel = 250,

    # Talent points gained per level after 60
    [float]$TalentPointsPerLevel = 2,

    # Blueprint points gained per level after 60
    [float]$BlueprintPointsPerLevel = 1,

    # Solo talent points gained per level after 60
    [float]$SoloPointsPerLevel = 1,

    # XP required per level after 60
    [int]$XpPerLevel = 144000
)

$ErrorActionPreference = "Stop"

$curvesDir = Join-Path $PSScriptRoot "..\curves"

New-Item `
    -ItemType Directory `
    -Force `
    -Path $curvesDir | Out-Null

function New-KeysFromLevel {
    param(
        [int]$From,
        [int]$To,
        [scriptblock]$ValueFn
    )

    $keys = @()

    for ($level = $From; $level -le $To; $level++) {

        $keys += @{
            time  = [float]$level
            value = [float](& $ValueFn $level)
        }
    }

    return $keys
}

# =========================================================
# VANILLA LEVEL 60 DATA
# =========================================================

#
# Based on actual ICARUS progression table:
#
# Level 60 Total XP:            5,330,000
# XP Required Per Level @ 60:     144,000
# Total Talent Points @ 60:            90
# Total Blueprint Points @ 60:        210
#

$baseXpAt60 = 5330000

$baseTalentAt60 = 90

$baseBlueprintAt60 = 210

#
# Solo talents are assumed to match
# total solo tree unlock progression.
#
$baseSoloAt60 = 60

# =========================================================
# TALENT CURVE
# =========================================================

$talentKeys = New-KeysFromLevel `
    -From 61 `
    -To $MaxLevel `
    -ValueFn {

    param($l)

    $baseTalentAt60 +
    (($l - 60) * $TalentPointsPerLevel)
}

@{
    assetName         = "C_PlayerTalentGrowth"
    extendFromVanilla = $true
    minPatchTime      = 61
    keys              = $talentKeys
} |
ConvertTo-Json -Depth 6 |
Set-Content (
    Join-Path $curvesDir "C_PlayerTalentGrowth.curve.json"
) -Encoding UTF8

# =========================================================
# BLUEPRINT CURVE
# =========================================================

$blueprintKeys = New-KeysFromLevel `
    -From 61 `
    -To $MaxLevel `
    -ValueFn {

    param($l)

    $baseBlueprintAt60 +
    (($l - 60) * $BlueprintPointsPerLevel)
}

@{
    assetName         = "C_PlayerBlueprintGrowth"
    extendFromVanilla = $true
    minPatchTime      = 61
    keys              = $blueprintKeys
} |
ConvertTo-Json -Depth 6 |
Set-Content (
    Join-Path $curvesDir "C_PlayerBlueprintGrowth.curve.json"
) -Encoding UTF8

# =========================================================
# SOLO TALENT CURVE
# =========================================================

$soloKeys = New-KeysFromLevel `
    -From 61 `
    -To $MaxLevel `
    -ValueFn {

    param($l)

    $baseSoloAt60 +
    (($l - 60) * $SoloPointsPerLevel)
}

@{
    assetName         = "C_SoloTalentGrowth"
    extendFromVanilla = $true
    minPatchTime      = 61
    keys              = $soloKeys
} |
ConvertTo-Json -Depth 6 |
Set-Content (
    Join-Path $curvesDir "C_SoloTalentGrowth.curve.json"
) -Encoding UTF8

# =========================================================
# XP CURVE
# =========================================================

#
# Flat progression continuation from vanilla.
#
# Example:
#
# Level 60 -> 61 = 144k XP
# Level 61 -> 62 = 144k XP
#
# etc.
#

$currentXp = $baseXpAt60

$xpKeys = @()

for ($level = 61; $level -le $MaxLevel; $level++) {

    $currentXp += $XpPerLevel

    $xpKeys += @{
        time  = [float]$level
        value = [float]$currentXp
    }
}

@{
    assetName         = "C_PlayerExperienceGrowth"
    extendFromVanilla = $true
    minPatchTime      = 61
    keys              = $xpKeys
} |
ConvertTo-Json -Depth 6 |
Set-Content (
    Join-Path $curvesDir "C_PlayerExperienceGrowth.curve.json"
) -Encoding UTF8

# =========================================================
# OUTPUT
# =========================================================

Write-Host ""
Write-Host "Generated ICARUS curve patches"
Write-Host "Output: $curvesDir"
Write-Host ""
Write-Host "Max Level: $MaxLevel"
Write-Host "XP Per Level After 60: $XpPerLevel"
Write-Host ""
Write-Host "Vanilla Level 60 Values:"
Write-Host "  XP:         $baseXpAt60"
Write-Host "  Talents:    $baseTalentAt60"
Write-Host "  Blueprints: $baseBlueprintAt60"
Write-Host ""
Write-Host "Progression continues linearly from vanilla."