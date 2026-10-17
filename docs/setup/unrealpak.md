# UnrealPak

UTool does not pack files itself. It writes a prepared tree and runs Epic's `UnrealPak.exe`. That binary is Licensed Technology under the Unreal Engine EULA. This repo does not contain it. See [NOTICES.md](../../NOTICES.md).

## How UTool finds it

In order:

1. An explicit path the build was given.
2. The environment variable `UTOOL_UNREALPAK`, if that file exists.
3. A previously extracted copy under `assets/UnrealPak/Engine/Binaries/Win64/UnrealPak.exe`.
4. A few well-known install paths, including `C:\Program Files\Epic Games\UE_4.27\Engine\Binaries\Win64\UnrealPak.exe` and `UE_5.4` in the same layout.

If `assets/UnrealPak.zip` exists, the first pack extracts it into `assets/UnrealPak/` when the exe is missing or older than the zip.

## Supply it

Either drop a zip you are allowed to keep locally:

```text
assets/UnrealPak.zip
```

or point at an engine you installed:

```powershell
$env:UTOOL_UNREALPAK = "C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealPak.exe"
```

`utool check Icarus` fails while the exe is missing. A mod build fails with `UnrealPak.exe not found` in that same case.

The extracted tree and the zip are gitignored. Do not commit them.
