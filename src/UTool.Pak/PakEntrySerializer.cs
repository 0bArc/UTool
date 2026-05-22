using UTool.Pak.IO;
using UTool.Pak.Models;

namespace UTool.Pak;

internal static class PakEntrySerializer
{
    public static int GetSerializedSize(int version, uint compressionMethodIndex, int compressionBlockCount = 0)
    {
        var size = 24L; // offset, compressed size, uncompressed size
        size += version < 5 ? 4 : 4;
        if (version <= 1)
            size += 8;
        size += 20;
        if (version >= 3 && compressionMethodIndex != 0)
            size += 4 + (compressionBlockCount * 16L);
        if (version >= 3)
            size += 5;
        return (int)size;
    }

    public static PakEntryRecord Read(UeBinaryReader reader, int version, string path)
    {
        var start = reader.BaseStream.Position;

        var offset = reader.ReadInt64();
        var size = reader.ReadInt64();
        var uncompressedSize = reader.ReadInt64();
        uint compressionMethodIndex;

        if (version < 5)
        {
            var legacy = reader.ReadInt32();
            compressionMethodIndex = legacy switch
            {
                0 => 0,
                1 => 1,
                2 => 2,
                _ => (uint)legacy,
            };
        }
        else
        {
            compressionMethodIndex = reader.ReadUInt32();
        }

        if (version <= 1)
            reader.ReadInt64();

        var hash = reader.ReadBytes(20);
        PakCompressedBlock[] compressionBlocks = [];
        byte flags = 0;
        uint compressionBlockSize = 0;

        if (version >= 3)
        {
            if (compressionMethodIndex != 0)
            {
                compressionBlocks = reader.ReadTArray(() =>
                {
                    var blockStart = reader.ReadInt64();
                    var blockEnd = reader.ReadInt64();
                    return new PakCompressedBlock(blockStart, blockEnd);
                });
            }

            flags = reader.ReadByte();
            compressionBlockSize = reader.ReadUInt32();
        }

        var serializedSize = GetSerializedSize(version, compressionMethodIndex, compressionBlocks.Length);
        _ = start;
        return new PakEntryRecord
        {
            Path = path,
            Offset = offset,
            Size = size,
            UncompressedSize = uncompressedSize,
            CompressionMethodIndex = compressionMethodIndex,
            Hash = hash,
            Flags = flags,
            CompressionBlockSize = compressionBlockSize,
            CompressionBlocks = compressionBlocks,
            SerializedEntrySize = serializedSize,
        };
    }

    public static void Write(UeBinaryWriter writer, PakEntryRecord entry)
    {
        writer.Write(entry.Offset);
        writer.Write(entry.Size);
        writer.Write(entry.UncompressedSize);
        writer.Write(entry.CompressionMethodIndex);
        writer.Write(entry.Hash);

        if (entry.CompressionMethodIndex != 0)
        {
            writer.WriteTArray(entry.CompressionBlocks.ToList(), block =>
            {
                writer.Write(block.CompressedStart);
                writer.Write(block.CompressedEnd);
            });
        }

        writer.Write(entry.Flags);
        writer.Write(entry.CompressionBlockSize);
    }
}
