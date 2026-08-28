# 02 — Configure your game

UTool reads game paths from `utool.json`.

## Create config

Copy the example next to your mods workspace (repo root or `examples/`):

```powershell
Copy-Item utool.json.example utool.json
```

Edit paths for your install:

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

UTool walks up from the mod directory until it finds `utool.json`.

## Verify

```powershell
utool check
utool check Icarus
utool check "D:\SteamLibrary\steamapps\common\Icarus\Icarus"
```

Exit code `0` = paks, `dataPak`, UnrealPak, and `@data` / `@paks` aliases resolve.

List configured games:

```powershell
utool games list
```

Probe an unknown install folder:

```powershell
utool games probe "D:\SteamLibrary\steamapps\common\Icarus\Icarus"
```

## Aliases in mod.lua

| Alias | Resolves to |
|-------|-------------|
| `@data` | `dataPak` |
| `@paks` | all `*.pak` under `paksDir` |

## Scaffold a new mod (optional)

Prints starter `mod.lua` to stdout — does not write files:

```powershell
utool auto setup Icarus --id icarus.my_mod --name "My Mod" > mod.lua
```

## Next

[03 — First mod](03-first-mod.md)
