using System.Buffers;
using System.Text;
using CsStratware.Pak.Models;

namespace CsStratware.Pak;

public static class StreamingPakGrep
{
    private const int ChunkSize = 256 * 1024;

    public static bool TrySearchEntry(
        Stream pakStream,
        PakEntryRecord entry,
        PakFooter footer,
        ReadOnlySpan<byte> needleUtf8,
        ReadOnlySpan<byte> needleUtf16,
        out int offset)
    {
        offset = -1;

        if (entry.IsEncrypted)
            return false;

        if (entry.IsCompressed)
        {
            var methodName = entry.CompressionMethodIndex < footer.CompressionMethods.Count
                ? footer.CompressionMethods[(int)entry.CompressionMethodIndex]
                : string.Empty;

            if (methodName.Contains("Oodle", StringComparison.OrdinalIgnoreCase))
                throw new NotSupportedException(
                    $"Oodle-compressed entry '{entry.Path}'. Use 'pak ue extract' or export via FModel.");

            // Compressed non-Oodle: fall back to full decompress path in caller.
            return false;
        }

        pakStream.Seek(entry.Offset + entry.SerializedEntrySize, SeekOrigin.Begin);
        var remaining = (int)Math.Min(entry.UncompressedSize, int.MaxValue);
        var carry = ArrayPool<byte>.Shared.Rent(ChunkSize + needleUtf8.Length);
        var carryLen = 0;

        try
        {
            while (remaining > 0)
            {
                var toRead = Math.Min(ChunkSize, remaining);
                var readBuf = carry.AsSpan(carryLen, toRead);
                var read = pakStream.Read(readBuf);
                if (read == 0)
                    break;

                remaining -= read;
                var window = carry.AsSpan(0, carryLen + read);
                offset = BinarySpanSearch.IndexOf(window, needleUtf8);
                if (offset < 0)
                    offset = BinarySpanSearch.IndexOf(window, needleUtf16);
                if (offset >= 0)
                    return true;

                var overlap = Math.Max(needleUtf8.Length, needleUtf16.Length) - 1;
                if (overlap > 0 && window.Length > overlap)
                {
                    window.Slice(window.Length - overlap).CopyTo(carry);
                    carryLen = overlap;
                }
                else
                {
                    carryLen = 0;
                }
            }
        }
        finally
        {
            ArrayPool<byte>.Shared.Return(carry);
        }

        return false;
    }

    public static (byte[] Utf8, byte[] Utf16) NeedleBytes(string needle) =>
        (Encoding.UTF8.GetBytes(needle), Encoding.Unicode.GetBytes(needle));
}
