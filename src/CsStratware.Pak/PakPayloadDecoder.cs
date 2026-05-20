using System.IO.Compression;

namespace CsStratware.Pak;

public static class PakPayloadDecoder
{
    public static byte[] FinishEntryPayload(
        byte[] raw,
        long uncompressedSize,
        bool isCompressedEntry,
        string? path = null)
    {
        if (raw.Length == 0)
            return raw;

        if (!isCompressedEntry)
        {
            var trimmed = TrimToUncompressedSize(raw, uncompressedSize);
            if (LooksLikeTextPayload(trimmed, path))
                return trimmed;

            if (TryZlibDecompress(trimmed, out var zlib) && LooksLikeTextPayload(zlib, path))
                return zlib;

            if (TryZlibDecompress(raw, out zlib) && LooksLikeTextPayload(zlib, path))
                return zlib;
        }

        return TrimToUncompressedSize(raw, uncompressedSize);
    }

    public static bool LooksLikeTextPayload(ReadOnlySpan<byte> data, string? path = null)
    {
        if (data.Length == 0)
            return false;

        var ext = path is null ? null : Path.GetExtension(path);
        if (ext is not null && IsBinaryExtension(ext))
            return true;

        var start = data[0];
        if (start is (byte)'{' or (byte)'[' or (byte)'"' or (byte)'#' or (byte)';')
            return true;

        if (start is >= (byte)'a' and <= (byte)'z' or >= (byte)'A' and <= (byte)'Z')
            return true;

        var sample = Math.Min(data.Length, 64);
        var nonPrintable = 0;
        for (var i = 0; i < sample; i++)
        {
            var b = data[i];
            if (b is < 0x09 or > 0x7E)
                nonPrintable++;
        }

        return nonPrintable < sample / 4;
    }

    public static bool TryZlibDecompress(ReadOnlySpan<byte> data, out byte[] decompressed)
    {
        decompressed = [];
        for (var offset = 0; offset < data.Length - 2; offset++)
        {
            if (data[offset] != 0x78)
                continue;
            if (data[offset + 1] is not (0x9C or 0x01 or 0x5E or 0xDA or 0x20))
                continue;

            try
            {
                using var input = new MemoryStream(data[offset..].ToArray());
                using var zlib = new ZLibStream(input, CompressionMode.Decompress);
                using var output = new MemoryStream();
                zlib.CopyTo(output);
                if (output.Length > 0)
                {
                    decompressed = output.ToArray();
                    return true;
                }
            }
            catch
            {
                // try next offset
            }
        }

        return false;
    }

    private static byte[] TrimToUncompressedSize(byte[] raw, long uncompressedSize)
    {
        if (uncompressedSize <= 0 || raw.Length <= uncompressedSize)
            return raw;

        var trimmed = new byte[uncompressedSize];
        Array.Copy(raw, trimmed, trimmed.Length);
        return trimmed;
    }

    private static bool IsBinaryExtension(string ext) =>
        ext.Equals(".uasset", StringComparison.OrdinalIgnoreCase)
        || ext.Equals(".uexp", StringComparison.OrdinalIgnoreCase)
        || ext.Equals(".ubulk", StringComparison.OrdinalIgnoreCase);
}
