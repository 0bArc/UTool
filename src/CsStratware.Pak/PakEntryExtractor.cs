using System.IO.Compression;
using System.Security.Cryptography;
using CsStratware.Pak.Models;

namespace CsStratware.Pak;

public static class PakEntryExtractor
{
    public static byte[] ReadEntry(Stream pakStream, PakEntryRecord entry, PakFooter footer)
    {
        if (entry.IsEncrypted)
            throw new NotSupportedException($"Encrypted pak entry not supported: {entry.Path}");

        if (entry.IsCompressed)
            return ReadCompressed(pakStream, entry, footer);

        pakStream.Seek(entry.Offset + entry.SerializedEntrySize, SeekOrigin.Begin);
        var data = new byte[entry.UncompressedSize];
        var read = pakStream.Read(data, 0, data.Length);
        if (read != data.Length)
            throw new EndOfStreamException($"Truncated pak entry: {entry.Path}");

        return data;
    }

    private static byte[] ReadCompressed(Stream pakStream, PakEntryRecord entry, PakFooter footer)
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
            var compressedSize = (int)(block.CompressedEnd - block.CompressedStart);
            var compressed = new byte[compressedSize];
            var blockOffset = useRelativeOffsets
                ? dataStart + block.CompressedStart
                : block.CompressedStart;
            pakStream.Seek(blockOffset, SeekOrigin.Begin);
            pakStream.ReadExactly(compressed);

            using var zlib = new ZLibStream(new MemoryStream(compressed), CompressionMode.Decompress);
            var blockRemaining = (int)Math.Min(entry.CompressionBlockSize, entry.UncompressedSize - outputOffset);
            var written = zlib.Read(output, outputOffset, blockRemaining);
            outputOffset += written;
        }

        return output;
    }

    public static void ExtractToDirectory(PakArchive archive, string outputDirectory, bool preservePaths = true)
    {
        Directory.CreateDirectory(outputDirectory);
        using var stream = File.OpenRead(archive.FilePath);

        foreach (var entry in archive.Entries.Values.Where(e => !e.IsDeleted))
        {
            var data = ReadEntry(stream, entry, archive.Footer);
            var relative = preservePaths
                ? NormalizeExtractPath(entry.Path, archive.MountPoint)
                : Path.GetFileName(entry.Path);

            var target = Path.Combine(outputDirectory, relative);
            Directory.CreateDirectory(Path.GetDirectoryName(target)!);
            File.WriteAllBytes(target, data);
        }
    }

    private static string NormalizeExtractPath(string entryPath, string mountPoint)
    {
        var path = entryPath;
        if (path.StartsWith(mountPoint, StringComparison.OrdinalIgnoreCase))
            path = path[mountPoint.Length..];

        path = path.TrimStart('/', '\\');
        return path.Replace('/', Path.DirectorySeparatorChar);
    }
}
