# 01 — Install and build

Get a working `utool.exe`.

## Requirements

- Windows (primary target)
- Visual Studio C++ build tools
- CMake
- Ninja (recommended)

`NMake` breaks when the repo path contains `#` (e.g. `c#`). Use Ninja and an out-of-tree build dir.

## Build

From the repo root:

```powershell
.\cmake\build-release.cmd
```

Output:

- `F:\Data\personal\utool-build\utool.exe` (or your configured build dir)
- `%LOCALAPPDATA%\utool\utool.exe` after install step in the script

## Add to PATH (once)

```powershell
$utoolBin = Join-Path $env:LOCALAPPDATA "utool"
[Environment]::SetEnvironmentVariable(
  "Path",
  $env:Path + ";" + $utoolBin,
  "User")
$env:Path += ";" + $utoolBin
utool --version
```

Or call the full path every time:

```powershell
& "F:\Data\personal\utool-build\utool.exe" --version
```

## UnrealPak bundle

Keep `assets/UnrealPak.zip` in the repo. First pack auto-extracts it under `assets/UnrealPak/`.

## Next

[02 — Configure your game](02-configure-game.md)
