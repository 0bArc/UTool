# Build csmanager to dist\csmanager and bump <Version> in Directory.Build.props each run.
param(
    [Alias("c")]
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",

    [switch]$NoBump
)

$ErrorActionPreference = "Stop"

function Find-RepoRoot {
    $dir = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
    for ($i = 0; $i -lt 12; $i++) {
        if (Test-Path (Join-Path $dir "csStratware.sln")) { return $dir }
        $parent = Split-Path $dir -Parent
        if (-not $parent -or $parent -eq $dir) { break }
        $dir = $parent
    }
    throw "Could not find csStratware.sln (run from repo root)."
}

function Bump-VersionInProps {
    param([string]$PropsPath)

    $text = [IO.File]::ReadAllText($PropsPath)
    if ($text -notmatch '<Version>(\d+)\.(\d+)\.(\d+)</Version>') {
        throw "No <Version>major.minor.patch</Version> in Directory.Build.props"
    }

    $major, $minor, $patch = [int]$Matches[1], [int]$Matches[2], [int]$Matches[3]
    $patch++
    $newVersion = "$major.$minor.$patch"
    $updated = [regex]::Replace(
        $text,
        '<Version>\d+\.\d+\.\d+</Version>',
        "<Version>$newVersion</Version>",
        1)

    if ($updated -eq $text) {
        throw "Failed to update Version in Directory.Build.props"
    }

    [IO.File]::WriteAllText($PropsPath, $updated)
    return $newVersion
}

$root = Find-RepoRoot
$props = Join-Path $root "Directory.Build.props"
$dist = Join-Path $root "dist\csmanager"
$exe = Join-Path $dist "csmanager.exe"

Push-Location $root
try {
    $version = if ($NoBump) {
        if ((Get-Content $props -Raw) -match '<Version>(\d+\.\d+\.\d+)</Version>') { $Matches[1] }
        else { "unknown" }
    }
    else {
        Bump-VersionInProps -PropsPath $props
    }

    Write-Host "repo: $root"
    Write-Host "version: $version"
    Write-Host "config: $Configuration"

    dotnet run --project (Join-Path $root "build.csproj") -c $Configuration -- --configuration $Configuration
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    Write-Host ""
    Write-Host "OK: $exe"
    Write-Host "PATH: add `"$dist`""
}
finally {
    Pop-Location
}
