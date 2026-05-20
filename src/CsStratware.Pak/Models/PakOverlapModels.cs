namespace CsStratware.Pak.Models;

public sealed class PakOverlapSource
{
    public required string PakPath { get; init; }
    public required string EntryPath { get; init; }
    public required string RelativePath { get; init; }
    public long UncompressedSize { get; init; }
    public string? ContentHash { get; init; }
}

public sealed class PakOverlapConflict
{
    public required string RelativePath { get; init; }
    public required IReadOnlyList<PakOverlapSource> Sources { get; init; }
    public bool IdenticalContent { get; init; }
}

public sealed class PakOverlapReport
{
    public required IReadOnlyList<string> PakPaths { get; init; }
    public required IReadOnlyList<PakOverlapConflict> Conflicts { get; init; }
    public int DistinctPaths { get; init; }
    public bool HasContentConflicts => Conflicts.Any(c => !c.IdenticalContent);
}
