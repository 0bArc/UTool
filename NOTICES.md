# Third-party notices

UTool depends on or integrates with the components below. This file satisfies attribution requirements for bundled dependencies and documents obligations for tools you must supply yourself.

## UAssetAPI

CurveFloat patching follows the cooked layout described by [UAssetAPI](https://github.com/atenfyr/UAssetAPI). UTool does not link or ship that library.

## Unreal Engine and UnrealPak

UTool can **invoke** Epic Games **UnrealPak** (`UnrealPak.exe`) when you supply it locally (`assets/UnrealPak.zip`, `UTOOL_UNREALPAK`, or an Unreal Engine install on the machine).

**UnrealPak is not part of UTool’s open-source release.** Engine tools are **Licensed Technology** under the [Unreal Engine End User License Agreement](https://www.unrealengine.com/en-US/eula/unreal). You must:

- Obtain Unreal Engine (or otherwise valid engine binaries) under Epic’s terms.
- Use UnrealPak only as permitted by that EULA (private development, packaging your own products, and other allowed uses described there).
- **Not** redistribute UnrealPak or other engine tools as a standalone product unless Epic’s EULA explicitly allows your situation.

UTool does **not** grant any license to Unreal Engine, UnrealPak, or Epic trademarks. **Unreal®** and **Unreal Engine®** are trademarks of Epic Games, Inc. UTool is not affiliated with, endorsed by, or sponsored by Epic Games.

If you ship a mod or tool that calls UnrealPak, ensure your end users understand they need their own compliant Unreal Engine install (or your Product’s distribution must follow Epic’s rules for embedded engine code).

## Your game content

Patches and rebuilt `.pak` / `.uasset` files target **your** installed games. You are responsible for complying with each game’s terms of use and modding policy. UTool does not grant rights to game IP.
