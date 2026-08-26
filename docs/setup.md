# Setup

Build `utool`, point `utool.json` at your game, pack a mod.

## Build

```powershell
.\cmake\build-release.cmd
# exe: F:\Data\personal\utool-build\utool.exe
```

Needs VS C++ tools + CMake. See [README.md](../README.md) (Ninja out-of-tree — `#` in `c#` paths breaks NMake).

## Config

Copy `utool.json.example` → `utool.json` next to your mods workspace (or walk-up from the mod dir):

```json
{
  "games": {
    "Icarus": {
      "paksDir": "D:\\SteamLibrary\\steamapps\\common\\Icarus\\Icarus\\Content\\Paks",
      "dataPak": "D:\\SteamLibrary\\steamapps\\common\\Icarus\\Icarus\\Content\\Data\\data.pak"
    }
  },
  "defaultMountPoint": "../../../Icarus/Content/"
}
```

UnrealPak: keep `assets/UnrealPak.zip` in the repo (auto-extracts on first pack). Details: [assets/README.md](../assets/README.md).

## Pack a mod

```powershell
utool pak build-mod examples/250cap
utool pak build-mod examples/example-mod
```

More: [UTOOL-WORKSPACE-GUIDE.md](UTOOL-WORKSPACE-GUIDE.md).
