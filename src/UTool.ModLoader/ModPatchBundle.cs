namespace UTool.ModLoader;

public sealed class ModPatchBundle
{
    public IReadOnlyList<CodeAssetPatch> AssetPatches { get; init; } = [];
    public IReadOnlyList<CodePlayerDataPatch> PlayerDataPatches { get; init; } = [];

    public bool HasWork => AssetPatches.Count > 0 || PlayerDataPatches.Count > 0;
}

public sealed class CodePlayerDataPatch
{
    public required string RelativePath { get; init; }
    public required Sdk.PlayerDataPatch Instance { get; init; }
    public required Type PatchType { get; init; }
}
