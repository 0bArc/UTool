namespace UTool.ModLoader.Merge;

public sealed class UeDataTableMergeOptions
{
    /// <summary>Label for conflict reports (asset file name or path).</summary>
    public string AssetLabel { get; init; } = "asset";

    /// <summary>Property name marking a row for deletion during merge.</summary>
    public string DeletionMarkerProperty { get; init; } = UeDataTableMerger.DeletionMarkerProperty;

    public bool NumericMinWins { get; init; } = true;

    public bool LaterModWinsNonNumeric { get; init; } = true;
}
