namespace CsStratware.Pak;

public static class PakFormat
{
    public const uint Magic = 0x5A6F12E1;
    public const int FooterSizeV11 = 221;
    public const int CompressionMethodNameLength = 32;
    public const int MaxCompressionMethods = 5;

    /// <summary>Legacy FString index (builder); v10+ path-hash index not written yet.</summary>
    public const int DefaultPakVersion = 9;
}

public enum PakCompressionMethod
{
    None = 0,
    Zlib = 1,
}
