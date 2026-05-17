using System.Text;
using CsStratware.Pak.IO;
using CsStratware.Pak.Models;

namespace CsStratware.Pak;

public static class PakFooterReader
{
    public static PakFooter Read(Stream stream)
    {
        if (!stream.CanSeek)
            throw new ArgumentException("Pak stream must be seekable.", nameof(stream));

        stream.Seek(-PakFormat.FooterSizeV11, SeekOrigin.End);
        using var reader = new UeBinaryReader(stream);

        var encryptionKeyGuid = new Guid(reader.ReadBytes(16));
        var encryptedIndex = reader.ReadByte() != 0;
        var magic = reader.ReadUInt32();
        if (magic != PakFormat.Magic)
            throw new InvalidDataException($"Invalid pak magic: 0x{magic:X8}");

        var version = reader.ReadInt32();
        var indexOffset = reader.ReadInt64();
        var indexSize = reader.ReadInt64();
        var indexHash = reader.ReadBytes(20);

        var methods = new List<string>(PakFormat.MaxCompressionMethods);
        var methodBuffer = reader.ReadBytes(PakFormat.CompressionMethodNameLength * PakFormat.MaxCompressionMethods);
        for (var i = 0; i < PakFormat.MaxCompressionMethods; i++)
        {
            var offset = i * PakFormat.CompressionMethodNameLength;
            if (methodBuffer[offset] == 0)
                continue;

            var end = offset + PakFormat.CompressionMethodNameLength;
            var len = 0;
            while (len < PakFormat.CompressionMethodNameLength && methodBuffer[offset + len] != 0)
                len++;

            methods.Add(Encoding.ASCII.GetString(methodBuffer, offset, len));
        }

        return new PakFooter
        {
            Version = version,
            IndexOffset = indexOffset,
            IndexSize = indexSize,
            IndexHash = indexHash,
            EncryptedIndex = encryptedIndex,
            EncryptionKeyGuid = encryptionKeyGuid,
            CompressionMethods = methods,
        };
    }
}
