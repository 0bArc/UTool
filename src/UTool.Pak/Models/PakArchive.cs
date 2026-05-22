namespace UTool.Pak.Models;

public sealed class PakArchive
{
    public required string FilePath { get; init; }
    public required PakFooter Footer { get; init; }
    public required string MountPoint { get; init; }
    public required IReadOnlyDictionary<string, PakEntryRecord> Entries { get; init; }
}
