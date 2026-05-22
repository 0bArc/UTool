namespace UTool.ModLoader.Merge;

public sealed class PropertyMergeConflict
{
    public required string AssetLabel { get; init; }
    public required string RowKey { get; init; }
    public required string PropertyPath { get; init; }
    public string? EarlierSource { get; init; }
    public required string LaterSource { get; init; }
    public string? EarlierValue { get; init; }
    public string? LaterValue { get; init; }
    public string? ResolvedValue { get; init; }
    /// <summary>numeric-min | later-wins | deletion</summary>
    public required string Resolution { get; init; }
}

public sealed class RowDeletionRecord
{
    public required string AssetLabel { get; init; }
    public required string RowKey { get; init; }
    public required string Source { get; init; }
}

public sealed class MergeConflictReport
{
    public required string AssetLabel { get; init; }
    public IReadOnlyList<PropertyMergeConflict> PropertyConflicts { get; init; } = [];
    public IReadOnlyList<RowDeletionRecord> RowDeletions { get; init; } = [];
    public int TotalConflicts => PropertyConflicts.Count;
}

public sealed class UeDataTableMergeResult
{
    public required string Json { get; init; }
    public required MergeConflictReport Report { get; init; }
}
