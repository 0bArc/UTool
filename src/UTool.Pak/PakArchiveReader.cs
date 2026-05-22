using UTool.Pak.IO;
using UTool.Pak.Models;

namespace UTool.Pak;

public static class PakArchiveReader
{
    public static PakArchive Open(string pakPath, PakOpenOptions? options = null)
    {
        var stream = File.OpenRead(pakPath);
        try
        {
            return Open(stream, pakPath, options);
        }
        catch
        {
            stream.Dispose();
            throw;
        }
    }

    public static PakArchive Open(Stream stream, string? displayPath = null, PakOpenOptions? options = null)
    {
        var footer = PakFooterReader.Read(stream);
        stream.Seek(footer.IndexOffset, SeekOrigin.Begin);
        var indexBytes = new byte[footer.IndexSize];
        var read = stream.Read(indexBytes, 0, indexBytes.Length);
        if (read != indexBytes.Length)
            throw new EndOfStreamException("Unexpected end of pak index.");

        if (footer.EncryptedIndex)
        {
            if (options?.AesKey is null)
            {
                throw new NotSupportedException(
                    "Encrypted pak index. Pass AES key via PAK_AES_KEY env, --aes-key, or PakOpenOptions.AesKey.");
            }

            indexBytes = PakAesHelper.DecryptIndex(indexBytes, options.AesKey);
        }

        Dictionary<string, PakEntryRecord> entries;
        string mountPoint;

        if (PakVersion.UsesPathHashIndex(footer.Version))
        {
            (mountPoint, entries) = PakModernIndexReader.Read(stream, footer, indexBytes);
        }
        else
        {
            using var indexStream = new MemoryStream(indexBytes);
            using var reader = new UeBinaryReader(indexStream);

            mountPoint = reader.ReadFString();
            var count = reader.ReadInt32();
            entries = new Dictionary<string, PakEntryRecord>(count, StringComparer.OrdinalIgnoreCase);

            for (var i = 0; i < count; i++)
            {
                var relativePath = reader.ReadFString();
                var fullPath = CombineMountPath(mountPoint, relativePath);
                var entry = PakEntrySerializer.Read(reader, footer.Version, fullPath);
                entries[fullPath] = entry;
            }
        }

        return new PakArchive
        {
            FilePath = displayPath ?? "(stream)",
            Footer = footer,
            MountPoint = mountPoint,
            Entries = entries,
        };
    }

    internal static string CombineMountPath(string mountPoint, string relativePath)
    {
        if (mountPoint.EndsWith('/') && relativePath.StartsWith('/'))
            return mountPoint + relativePath[1..];

        if (!mountPoint.EndsWith('/') && !relativePath.StartsWith('/'))
            return mountPoint + relativePath;

        return mountPoint + relativePath;
    }
}
