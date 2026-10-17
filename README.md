# UTool

Windows CLI for UE4/UE5 mods. You write `mod.lua`. UTool patches JSON and curves out of the game pak. UnrealPak writes a `*_P.pak`.

Manual: [docs/README.md](docs/README.md).

```powershell
.\cmake\build-release.cmd
utool --version
Copy-Item utool.json.example utool.json
utool check Icarus
```

`utool` with no args opens a menu. `utool help`, `utool mod help`, and `utool pak help` list commands.

The mod language is [docs/mods/README.md](docs/mods/README.md). Personal mods stay in `examples/` on your machine. That folder is not in this repo.

`utool.exe` for version 0.2.0 is on the GitHub release. You still supply UnrealPak locally. See [docs/setup/unrealpak.md](docs/setup/unrealpak.md).
