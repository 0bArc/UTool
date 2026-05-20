namespace CsStratware.Pak;

public sealed class PakMergeOptions
{
    public PakOpenOptions? PakOpenOptions { get; init; }

    /// <summary>When true, colliding .json entries are merged (UE Rows union) instead of last-wins.</summary>
    public bool JsonMerge { get; init; } = true;

    public PakBuildOptions? BuildOptions { get; init; }
}
