# Install

Windows. Visual Studio with the Desktop development with C++ workload, plus CMake. Ninja ships with the Visual Studio CMake tools.

`NMake` fails when the repo path contains `#` (a path like `c#\csStratware`). The scripts below never build inside the repo.

## Release binary

GitHub Releases has `utool.exe` for the current version. Put that file in `%LOCALAPPDATA%\utool\` and add that directory to PATH. You still need UnrealPak on the machine. See [UnrealPak](unrealpak.md).

## Build from source

From the repo root:

```powershell
.\cmake\build-release.cmd
```

The script:

1. Finds the newest Visual Studio with the C++ tools via `vswhere`.
2. Loads `vcvars64.bat`.
3. Configures Ninja in `%LOCALAPPDATA%\utool-build`.
4. Builds `utool.exe`.
5. Copies it to `%LOCALAPPDATA%\utool\utool.exe`.

PATH, once per machine:

```powershell
$utoolBin = Join-Path $env:LOCALAPPDATA "utool"
[Environment]::SetEnvironmentVariable("Path", $env:Path + ";" + $utoolBin, "User")
$env:Path += ";" + $utoolBin
utool --version
```

After the first configure, incremental builds:

```powershell
.\cmake\rebuild.cmd
```

`rebuild.cmd` does not reconfigure. Run `build-release.cmd` again after you pull CMake or dependency changes.

## What you should see

`utool --version` prints `0.2.0` (or whatever `include/UTool/Version.hpp` says in the commit you built).

`utool help` lists `mod`, `pak`, `games`, and `check`. `utool mod help` and `utool pak help` list those subcommands. Those three texts are the contract. If a doc and the binary disagree, the binary is right.
