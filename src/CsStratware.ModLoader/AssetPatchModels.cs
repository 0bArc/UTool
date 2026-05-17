namespace CsStratware.ModLoader;

public sealed class AssetPatchDocument
{
    public IReadOnlyList<AssetPatch> Patches { get; init; } = [];
}

public sealed class AssetPatch
{
    public required string AssetPath { get; init; }
    public IReadOnlyList<PatchOperation> Operations { get; init; } = [];
}

public sealed class PatchOperation
{
    public required string Op { get; init; }
    public required string Path { get; init; }
    public object? Value { get; init; }
}
