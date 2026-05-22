namespace UTool.Pak.Models;

public sealed class PakEntryRecord
{
    public required string Path { get; init; }
    public required long Offset { get; init; }
    public required long Size { get; init; }
    public required long UncompressedSize { get; init; }
    public required uint CompressionMethodIndex { get; init; }
    public required byte[] Hash { get; init; }
    public required byte Flags { get; init; }
    public required uint CompressionBlockSize { get; init; }
    public required IReadOnlyList<PakCompressedBlock> CompressionBlocks { get; init; }
    public required int SerializedEntrySize { get; init; }

    public bool IsEncrypted => (Flags & 0x01) != 0;
    public bool IsDeleted => (Flags & 0x02) != 0;
    public bool IsCompressed => CompressionMethodIndex != 0;
}

public readonly struct PakCompressedBlock
{
    public PakCompressedBlock(long start, long end)
    {
        CompressedStart = start;
        CompressedEnd = end;
    }

    public long CompressedStart { get; }
    public long CompressedEnd { get; }
}
