# Local assets (not in git)

**Legal:** `UnrealPak` and other engine binaries are Epic **Licensed Technology** under the [Unreal Engine EULA](https://www.unrealengine.com/en-US/eula/unreal). Do not redistribute them except as Epic permits. UTool only runs `UnrealPak.exe` if you install it locally. See [notices](../docs/notices.md).

Place **`UnrealPak.zip`** in this folder (it is not in git). First pack extracts it to **`UnrealPak/`**.

Or point at an engine binary:

```powershell
$env:UTOOL_UNREALPAK = "C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealPak.exe"
```
