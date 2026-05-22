using System.IO.Compression;
using UTool.Pak.Models;

namespace UTool.Pak;

public static class PakEntryExtractor
{
    public static byte[] ReadEntry(
        Stream pakStream,
        PakEntryRecord entry,
        PakFooter footer,
        byte[]? aesKey = null)
    {
        if (entry.IsEncrypted && aesKey is null)
        {
            throw new NotSupportedException(
                $"Encrypted pak entry '{entry.Path}'. Set pakAesKey in utool.json, --aes-key, or PAK_AES_KEY.");
        }

        if (entry.IsCompressed)
            return ReadCompressed(pakStream, entry, footer, aesKey);

        var storedSize = entry.Size > 0 ? entry.Size : entry.UncompressedSize;
        var readSize = entry.IsEncrypted ? PakAesHelper.Align16(storedSize) : storedSize;
        pakStream.Seek(entry.Offset + entry.SerializedEntrySize, SeekOrigin.Begin);
        var raw = new byte[readSize];
        pakStream.ReadExactly(raw);

        if (entry.IsEncrypted)
            raw = PakAesHelper.DecryptData(raw, aesKey!);
        else if (aesKey is not null && !PakPayloadDecoder.LooksLikeTextPayload(raw, entry.Path))
            raw = TryDecryptOpaquePayload(raw, aesKey);

        return PakPayloadDecoder.FinishEntryPayload(raw, entry.UncompressedSize, isCompressedEntry: false, entry.Path);
    }

    private static byte[] ReadCompressed(
        Stream pakStream,
        PakEntryRecord entry,
        PakFooter footer,
        byte[]? aesKey)
    {
        var methodName = entry.CompressionMethodIndex < footer.CompressionMethods.Count
            ? footer.CompressionMethods[(int)entry.CompressionMethodIndex]
            : string.Empty;

        if (methodName.Contains("Oodle", StringComparison.OrdinalIgnoreCase))
        {
            throw new NotSupportedException(
                $"Oodle compression on '{entry.Path}'. Use 'pak ue extract' or FModel export.");
        }

        if (!methodName.Contains("Zlib", StringComparison.OrdinalIgnoreCase)
            && entry.CompressionMethodIndex != 1)
        {
            throw new NotSupportedException(
                $"Compression method '{methodName}' (index {entry.CompressionMethodIndex}) not supported for {entry.Path}. Use UnrealPak extract.");
        }

        var output = new byte[entry.UncompressedSize];
        var outputOffset = 0;

        var dataStart = entry.Offset + entry.SerializedEntrySize;
        var useRelativeOffsets = PakVersion.GetMajor(footer.Version) >= PakVersionMajor.RelativeChunkOffsets;

        foreach (var block in entry.CompressionBlocks)
        {
            var storedSize = (int)(block.CompressedEnd - block.CompressedStart);
            var readSize = entry.IsEncrypted ? (int)PakAesHelper.Align16(storedSize) : storedSize;
            var compressed = new byte[readSize];
            var blockOffset = useRelativeOffsets
                ? dataStart + block.CompressedStart
                : block.CompressedStart;
            pakStream.Seek(blockOffset, SeekOrigin.Begin);
            pakStream.ReadExactly(compressed);

            if (entry.IsEncrypted)
            {
                compressed = PakAesHelper.DecryptData(compressed, aesKey!);
                if (compressed.Length > storedSize)
                {
                    var trimmed = new byte[storedSize];
                    Array.Copy(compressed, trimmed, storedSize);
                    compressed = trimmed;
                }
            }

            using var zlib = new ZLibStream(new MemoryStream(compressed), CompressionMode.Decompress);
            var blockRemaining = (int)Math.Min(entry.CompressionBlockSize, entry.UncompressedSize - outputOffset);
            var written = zlib.Read(output, outputOffset, blockRemaining);
            outputOffset += written;
        }

        return PakPayloadDecoder.FinishEntryPayload(output, entry.UncompressedSize, isCompressedEntry: true, entry.Path);
    }

    public static void ExtractToDirectory(
        PakArchive archive,
        string outputDirectory,
        bool preservePaths = true,
        byte[]? aesKey = null)
    {
        Directory.CreateDirectory(outputDirectory);
        using var stream = File.OpenRead(archive.FilePath);

        foreach (var entry in archive.Entries.Values.Where(e => !e.IsDeleted))
        {
            var data = ReadEntry(stream, entry, archive.Footer, aesKey);
            var relative = preservePaths
                ? NormalizeExtractPath(entry.Path, archive.MountPoint)
                : Path.GetFileName(entry.Path);

            var target = Path.Combine(outputDirectory, relative);
            Directory.CreateDirectory(Path.GetDirectoryName(target)!);
            File.WriteAllBytes(target, data);
        }
    }

    private static byte[] TryDecryptOpaquePayload(byte[] raw, byte[] aesKey)
    {
        if (raw.Length == 0 || raw.Length % 16 != 0)
        {
            var padded = new byte[PakAesHelper.Align16(raw.Length)];
            Array.Copy(raw, padded, raw.Length);
            raw = padded;
        }

        try
        {
            return PakAesHelper.DecryptData(raw, aesKey);
        }
        catch
        {
            return raw;
        }
    }

    public static string NormalizeExtractPath(string entryPath, string mountPoint)
    {
        var path = entryPath;
        if (path.StartsWith(mountPoint, StringComparison.OrdinalIgnoreCase))
            path = path[mountPoint.Length..];

        path = path.TrimStart('/', '\\');
        return path.Replace('/', Path.DirectorySeparatorChar);
    }
}
