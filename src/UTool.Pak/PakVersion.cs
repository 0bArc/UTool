namespace UTool.Pak;

internal enum PakVersionMajor
{
    Unknown = 0,
    Initial = 1,
    NoTimestamps = 2,
    CompressionEncryption = 3,
    IndexEncryption = 4,
    RelativeChunkOffsets = 5,
    DeleteRecords = 6,
    EncryptionKeyGuid = 7,
    FNameBasedCompression = 8,
    FrozenIndex = 9,
    PathHashIndex = 10,
    Fnv64BugFix = 11,
}

internal static class PakVersion
{
    public static PakVersionMajor GetMajor(int version) => version switch
    {
        0 => PakVersionMajor.Unknown,
        1 => PakVersionMajor.Initial,
        2 => PakVersionMajor.NoTimestamps,
        3 => PakVersionMajor.CompressionEncryption,
        4 => PakVersionMajor.IndexEncryption,
        5 => PakVersionMajor.RelativeChunkOffsets,
        6 => PakVersionMajor.DeleteRecords,
        7 => PakVersionMajor.EncryptionKeyGuid,
        8 => PakVersionMajor.FNameBasedCompression,
        9 => PakVersionMajor.FrozenIndex,
        10 => PakVersionMajor.PathHashIndex,
        11 => PakVersionMajor.Fnv64BugFix,
        _ => version >= 11 ? PakVersionMajor.Fnv64BugFix : PakVersionMajor.Unknown,
    };

    public static bool UsesPathHashIndex(int version) => GetMajor(version) >= PakVersionMajor.PathHashIndex;
}
