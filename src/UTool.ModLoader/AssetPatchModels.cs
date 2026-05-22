namespace UTool.ModLoader;

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
    /// <summary>With removewhere/setwhere/removepropertywhere: property on each array element to match.</summary>
    public string? MatchProperty { get; init; }
    /// <summary>With removewhere/setwhere/removepropertywhere: value that <see cref="MatchProperty"/> must equal.</summary>
    public object? MatchValue { get; init; }
    /// <summary>With setwhere/removepropertywhere: JSON pointer relative to each matched element (e.g. /Metadata or Metadata).</summary>
    public string? TargetPath { get; init; }
}
