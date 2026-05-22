using UTool.Pak.IO;
using UTool.Pak.Models;

namespace UTool.Pak;

internal static class PakModernIndexReader
{
    public static (string MountPoint, Dictionary<string, PakEntryRecord> Entries) Read(
        Stream pakStream,
        PakFooter footer,
        byte[] indexBytes)
    {
        using var indexStream = new MemoryStream(indexBytes);
        using var reader = new UeBinaryReader(indexStream);

        var mountPoint = reader.ReadFString();
        _ = reader.ReadInt32(); // record count (informational)

        var pathHashSeed = reader.ReadUInt64();

        var hasPathHashIndex = reader.ReadInt32() != 0;
        if (hasPathHashIndex)
        {
            var pathHashIndexOffset = reader.ReadInt64();
            var pathHashIndexSize = reader.ReadInt64();
            reader.ReadBytes(20);
            SkipSecondaryBlob(pakStream, footer, pathHashIndexOffset, pathHashIndexSize);
        }

        var hasFullDirectoryIndex = reader.ReadInt32() != 0;
        Dictionary<string, int> directoryIndex;
        if (hasFullDirectoryIndex)
        {
            var fullDirectoryIndexOffset = reader.ReadInt64();
            var fullDirectoryIndexSize = reader.ReadInt64();
            reader.ReadBytes(20);
            directoryIndex = ReadFullDirectoryIndex(pakStream, footer, fullDirectoryIndexOffset, fullDirectoryIndexSize);
        }
        else
        {
            directoryIndex = [];
        }

        var encodedSize = reader.ReadInt32();
        var encodedBytes = reader.ReadBytes(encodedSize);

        var nonEncodedCount = reader.ReadInt32();
        var nonEncoded = new List<PakEntryRecord>(nonEncodedCount);
        for (var i = 0; i < nonEncodedCount; i++)
        {
            var path = reader.ReadFString();
            var entry = PakEntrySerializer.Read(reader, footer.Version, path);
            nonEncoded.Add(entry);
        }

        _ = pathHashSeed;

        using var encodedStream = new MemoryStream(encodedBytes);
        var entries = new Dictionary<string, PakEntryRecord>(directoryIndex.Count, StringComparer.OrdinalIgnoreCase);

        foreach (var (relativePath, encodedOffset) in directoryIndex)
        {
            if (PakEncodedEntryDecoder.IsDeletedOffset(encodedOffset))
                continue;

            PakEntryRecord entry;
            if (encodedOffset >= 0)
            {
                encodedStream.Position = encodedOffset;
                using var encodedReader = new UeBinaryReader(encodedStream);
                if (!PakEncodedEntryDecoder.TryRead(encodedReader, footer.Version, out var decoded) || decoded is null)
                    throw new InvalidDataException($"Failed to decode pak entry at offset {encodedOffset}.");

                entry = CloneEntry(decoded, CombineMountPath(mountPoint, relativePath));
            }
            else
            {
                var index = (-encodedOffset) - 1;
                if (index < 0 || index >= nonEncoded.Count)
                    throw new InvalidDataException($"Invalid non-encoded entry index {index} for {relativePath}.");

                entry = CloneEntry(nonEncoded[index], CombineMountPath(mountPoint, relativePath));
            }

            entries[entry.Path] = entry;
        }

        return (mountPoint, entries);
    }

    private static Dictionary<string, int> ReadFullDirectoryIndex(
        Stream pakStream,
        PakFooter footer,
        long offset,
        long size)
    {
        var bytes = ReadSecondaryBlob(pakStream, footer, offset, size);
        using var stream = new MemoryStream(bytes);
        using var reader = new UeBinaryReader(stream);

        var dirCount = reader.ReadInt32();
        var paths = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);

        for (var d = 0; d < dirCount; d++)
        {
            var directory = reader.ReadFString();
            var fileCount = reader.ReadInt32();
            for (var f = 0; f < fileCount; f++)
            {
                var fileName = reader.ReadFString();
                var encodedOffset = reader.ReadInt32();
                var relative = directory + fileName;
                paths[relative] = encodedOffset;
            }
        }

        return paths;
    }

    private static void SkipSecondaryBlob(Stream pakStream, PakFooter footer, long offset, long size)
    {
        _ = ReadSecondaryBlob(pakStream, footer, offset, size);
    }

    private static byte[] ReadSecondaryBlob(Stream pakStream, PakFooter footer, long offset, long size)
    {
        if (footer.EncryptedIndex)
            throw new NotSupportedException("Encrypted pak index not supported.");

        pakStream.Seek(offset, SeekOrigin.Begin);
        var bytes = new byte[size];
        pakStream.ReadExactly(bytes);
        return bytes;
    }

    private static string CombineMountPath(string mountPoint, string relativePath)
        => PakArchiveReader.CombineMountPath(mountPoint, relativePath);

    private static PakEntryRecord CloneEntry(PakEntryRecord source, string path) => new()
    {
        Path = path,
        Offset = source.Offset,
        Size = source.Size,
        UncompressedSize = source.UncompressedSize,
        CompressionMethodIndex = source.CompressionMethodIndex,
        Hash = source.Hash,
        Flags = source.Flags,
        CompressionBlockSize = source.CompressionBlockSize,
        CompressionBlocks = source.CompressionBlocks,
        SerializedEntrySize = source.SerializedEntrySize,
    };
}
