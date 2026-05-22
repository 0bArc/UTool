namespace UTool.Pak;

internal static class BinarySpanSearch
{
    public static int IndexOf(ReadOnlySpan<byte> haystack, ReadOnlySpan<byte> needle) =>
        haystack.IndexOf(needle);

    public static bool Contains(ReadOnlySpan<byte> haystack, ReadOnlySpan<byte> needle) =>
        IndexOf(haystack, needle) >= 0;
}
