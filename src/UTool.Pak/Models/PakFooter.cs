namespace UTool.Pak.Models;

public sealed class PakFooter
{
    public required int Version { get; init; }
    public required long IndexOffset { get; init; }
    public required long IndexSize { get; init; }
    public required byte[] IndexHash { get; init; }
    public required bool EncryptedIndex { get; init; }
    public required Guid EncryptionKeyGuid { get; init; }
    public required IReadOnlyList<string> CompressionMethods { get; init; }
}
