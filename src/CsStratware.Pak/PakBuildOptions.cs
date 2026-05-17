namespace CsStratware.Pak;

public sealed class PakBuildOptions
{
    /// <summary>UE mount point, e.g. ../../../YourGame/</summary>
    public string MountPoint { get; init; } = "../../../YourGame/";

    public int PakVersion { get; init; } = PakFormat.DefaultPakVersion;

    public PakCompressionMethod Compression { get; init; } = PakCompressionMethod.None;
}

public sealed class PakBuildResult
{
    public required string OutputPath { get; init; }
    public required int FileCount { get; init; }
    public required long TotalBytes { get; init; }
}
