# UTool

C++20 UE4/UE5 modding toolkit. Author mods in **Lua**, pack with **UnrealPak**.

Version **0.2.0**.

## Layout

```text
include/UTool/   public headers (Core, Pak, Mod, Lua, Cli)
src/             implementation
examples/        sample mods (250cap, morexp, example-mod)
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

1. Copy `utool.json.example` → `utool.json` and set game pak paths.
2. Ensure `assets/UnrealPak.zip` is present (auto-extracts on first pack).
3. Write a mod with `mod.json` + `scripts/*.lua` (+ optional `patches/`, `content/`).
4. Run:

```text
utool discover <mods-dir>
utool validate <mods-dir>
utool pak build-mod <mod-dir>
```

## Lua API

See **[docs/lua.md](docs/lua.md)** for the full surface (`utool.patch_asset` / `utool.patch_curve`, `utool.editor`, `utool.curve`).

```lua
utool.patch_curve("C_PlayerExperienceGrowth", "Data/Character", function()
  local last = utool.curve:LastKey()
  utool.curve:AddKey(last.Time + 1, last.Value + 144000)
end)

utool.patch_asset("D_CharacterGrowth.json", function()
  utool.editor:SetOnArrayElementsWhere("/Rows", "Name", "Player", "/MaxDisplayLevel", 250)
end)
```

Samples: [examples/250cap](examples/250cap), [examples/morexp](examples/morexp). Workspace guide: [docs/UTOOL-WORKSPACE-GUIDE.md](docs/UTOOL-WORKSPACE-GUIDE.md).
