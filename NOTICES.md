# Third-party notices

UTool depends on or integrates with the components below. This file satisfies attribution requirements for bundled dependencies and documents obligations for tools you must supply yourself.

## UAssetAPI

UTool uses [UAssetAPI](https://github.com/atenfyr/UAssetAPI) (NuGet: `UAssetAPI`) to read and write Unreal Engine `.uasset` files (for example `CurveFloat` patching in `UTool.ModLoader`).

UAssetAPI is distributed under the **MIT License**:

```
MIT License

Copyright (c) 2020 - 2026 atenfyr

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

Source: https://github.com/atenfyr/UAssetAPI/blob/master/LICENSE

## Unreal Engine and UnrealPak

UTool can **invoke** Epic Games **UnrealPak** (`UnrealPak.exe`) and related engine binaries when you install or point at a local Unreal Engine toolchain (for example `utool setup unrealpak`, `assets/UnrealPak.zip`, or `UTOOL_UNREALPAK`).

**UnrealPak is not part of UTool’s open-source release.** Engine tools are **Licensed Technology** under the [Unreal Engine End User License Agreement](https://www.unrealengine.com/en-US/eula/unreal). You must:

- Obtain Unreal Engine (or otherwise valid engine binaries) under Epic’s terms.
- Use UnrealPak only as permitted by that EULA (private development, packaging your own products, and other allowed uses described there).
- **Not** redistribute UnrealPak or other engine tools as a standalone product unless Epic’s EULA explicitly allows your situation.

UTool does **not** grant any license to Unreal Engine, UnrealPak, or Epic trademarks. **Unreal®** and **Unreal Engine®** are trademarks of Epic Games, Inc. UTool is not affiliated with, endorsed by, or sponsored by Epic Games.

If you ship a mod or tool that calls UnrealPak, ensure your end users understand they need their own compliant Unreal Engine install (or your Product’s distribution must follow Epic’s rules for embedded engine code).

## Your game content

Patches and rebuilt `.pak` / `.uasset` files target **your** installed games. You are responsible for complying with each game’s terms of use and modding policy. UTool does not grant rights to game IP.
