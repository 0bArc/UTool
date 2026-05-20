namespace CsStratware.ModLoader.Merge;

/// <summary>Declarative JSON patch op with mod source metadata (merge pipeline).</summary>
public enum JsonPatchKind
{
    Replace,
    Add,
    Remove,
    ReplaceAll,
    RemoveWhere,
    SetWhere,
    RemovePropertyWhere,
    DeleteRow,
}

public sealed class JsonPatchOperation
{
    public required JsonPatchKind Kind { get; init; }
    public required string SourceModId { get; init; }
    public required string AssetPath { get; init; }
    public required string Path { get; init; }
    public object? Value { get; init; }
    public string? MatchProperty { get; init; }
    public object? MatchValue { get; init; }
    public string? TargetPath { get; init; }

    public static JsonPatchOperation FromPatchOperation(
        string sourceModId,
        string assetPath,
        PatchOperation op) => new()
    {
        SourceModId = sourceModId,
        AssetPath = assetPath,
        Kind = ParseKind(op.Op),
        Path = op.Path,
        Value = op.Value,
        MatchProperty = op.MatchProperty,
        MatchValue = op.MatchValue,
        TargetPath = op.TargetPath,
    };

    private static JsonPatchKind ParseKind(string op) => op.ToLowerInvariant() switch
    {
        "replace" => JsonPatchKind.Replace,
        "add" => JsonPatchKind.Add,
        "remove" => JsonPatchKind.Remove,
        "replaceall" => JsonPatchKind.ReplaceAll,
        "removewhere" => JsonPatchKind.RemoveWhere,
        "setwhere" => JsonPatchKind.SetWhere,
        "removepropertywhere" => JsonPatchKind.RemovePropertyWhere,
        "deleterow" => JsonPatchKind.DeleteRow,
        _ => throw new NotSupportedException($"Unknown patch op: {op}"),
    };
}

public sealed class JsonModificationSet
{
    public required string SourceModId { get; init; }
    public required string AssetPath { get; init; }
    public IReadOnlyList<JsonPatchOperation> Operations { get; init; } = [];
}
