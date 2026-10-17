# Configure

UTool reads `utool.json`. From a mod directory it walks parents until it finds one. The usual place is the repo root, next to `examples/`.

```powershell
Copy-Item utool.json.example utool.json
```

`utool.json` is gitignored. The example file is the schema.

## Minimum file

```json
{
  "updateVersion": "1",
  "extractedDir": ".extracted",
  "defaultMountPoint": "../../../Icarus/Content/",
  "games": {
    "Icarus": {
      "paksDir": "D:\\SteamLibrary\\steamapps\\common\\Icarus\\Icarus\\Content\\Paks",
      "dataPak": "D:\\SteamLibrary\\steamapps\\common\\Icarus\\Icarus\\Content\\Data\\data.pak",
      "playerDataDir": "C:\\Users\\you\\AppData\\Local\\Icarus\\Saved\\PlayerData"
    }
  }
}
```

| Key | Role |
|-----|------|
| `updateVersion` | Default week string for `%updateversion%` when `mod.lua` omits `updateVersion` |
| `extractedDir` | Where extracted game JSON is cached |
| `defaultMountPoint` | Used when a mod sets `mountPoint = "@auto"` |
| `pakAesKey` | AES-256 key, 64 hex chars or base64, when the game paks are encrypted |
| `games.<id>.paksDir` | Folder of `*.pak`. Deploy copies into `<paksDir>/mods` |
| `games.<id>.dataPak` | The pak `@data` reads |
| `games.<id>.playerDataDir` | Saved player data, when a command needs it |
| `games.<id>.pakAesKey` | Per-game override of the root key |
| `lastShippedWeek` | Written by a full `utool mod ship --upload`. Do not hand-edit unless you mean to unblock that week |
| `catalog` | Rows for `-f` and `utool mod ship`. See [Ship](../cli/ship.md) |
| `nexus` | Upload map. See [Ship](../cli/ship.md) |

Backslashes in JSON paths are escaped (`\\`).

## Check

```powershell
utool check Icarus
utool games list
utool check "D:\SteamLibrary\steamapps\common\Icarus\Icarus"
utool games probe "D:\SteamLibrary\steamapps\common\Icarus\Icarus"
```

Exit code 0 means the game paths and UnrealPak resolve. A missing `data.pak` or a missing packer is a failed check, not a successful build that produces an empty pak.

## Aliases inside mod.lua

| Alias | Resolves to |
|-------|-------------|
| `@data` | That game's `dataPak` |
| `@paks` | Every `*.pak` in `paksDir` |
| `@auto` | `defaultMountPoint` (and the game path) at build time |

`@auto` is replaced before UnrealPak runs. The pak does not contain the text `@auto`.

`sourcePak` is where JSON and loose assets are read. `curveSourcePak` is where `.uasset` curves are read. They are often different: tables in `data.pak`, curves in the main pak chunk.

## Catalog

`utool mod build <folder>` does not need a catalog row. `utool mod list`, `-f`, and `utool mod ship` do.

```json
"catalog": [
  {
    "id": "morexp",
    "path": "progression/morexp",
    "category": "progression",
    "ship": false
  }
]
```

`path` is relative to the mods directory. That directory is `examples/` when it exists, otherwise the current directory. `id` is the `-f` key and the folder name `utool mod new` creates. It is not the Nexus page number.

`utool mod new` appends a row. `"ship": true` includes the mod in `utool mod ship`.
