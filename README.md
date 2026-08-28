# UTool

C++20 UE4/UE5 modding toolkit. Author mods in **Lua**, pack with **UnrealPak**.

Version **0.2.0**.

## Layout

```text
include/UTool/   public headers (Core, Pak, Mod, Lua, Cli)
src/             implementation
examples/        sample mods (250cap, morexp, affliction_chance)
assets/          UnrealPak.zip / extracted toolchain
docs/            guides
```

## Build

`NMake` breaks on the `#` in `c#` paths. Use **Ninja** and an out-of-tree build dir:

```powershell
.\cmake\build-release.cmd
# build:   F:\Data\personal\utool-build\utool.exe
# install: %LOCALAPPDATA%\utool\utool.exe
```

Put install dir on PATH (once per machine):

```powershell
$utoolBin = Join-Path $env:LOCALAPPDATA "utool"
[Environment]::SetEnvironmentVariable(
  "Path",
  $env:Path + ";" + $utoolBin,
  "User")
$env:Path += ";" + $utoolBin   # current session
utool --version
```

## Quick start

Step-by-step guides: **[docs/README.md](docs/README.md)** (01 install → 08 Pak Studio).

```powershell
Copy-Item utool.json.example utool.json   # edit game paths
utool check Icarus
utool pak build-mod examples/morexp --force-extract
```

## Pak debugger (browse / preview)

List, search, preview, and extract assets from game paks without bulk extraction:

```text
utool pak list @paks --ext json --json
utool pak search CharacterGrowth --from Icarus --json
utool pak preview Data/Character/D_CharacterGrowth.json --from @data --json
utool pak snippet Data/Character/D_CharacterGrowth.json --from @data --row Player --field MaxDisplayLevel
```

Desktop UI: [tools/PakStudio](tools/PakStudio) (Next.js). Pick a game from `utool.json` or scan an install folder. VS Code: **UTool: Browse Game Paks**.

## Lua API

See **[docs/05-lua-modding.md](docs/05-lua-modding.md)** and **[docs/07-multi-variant-mods.md](docs/07-multi-variant-mods.md)** (`:Value`, `:zip`).
Find game tables: **[docs/04-find-assets.md](docs/04-find-assets.md)**.

```lua
utool.mod {
  id = "icarus.example",
  name = "Example",
  pak = {
    output = "dist/example_P.pak",
    mountPoint = "../../../Icarus/Content/",
    sourcePak = "@data",
  },
}

utool.asset("D_AfflictionChance.json")
  :row("Underground_Escalation")
  :field("ChanceInPercent")
  :set(0)
```

Samples: [examples/250cap](examples/250cap), [examples/morexp](examples/morexp), [examples/affliction_chance](examples/affliction_chance), [examples/freshrations](examples/freshrations), [examples/beasttamer](examples/beasttamer).
