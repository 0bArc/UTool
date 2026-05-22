using UTool.Pak.IO;
using UTool.Pak.Models;

namespace UTool.Pak;

internal static class PakEncodedEntryDecoder
{
    private const int DeletedEntrySentinel = int.MinValue;

    public static bool TryRead(UeBinaryReader reader, int version, out PakEntryRecord? entry)
    {
        entry = null;
        var bits = reader.ReadUInt32();

        var compressionSlot = (bits >> 23) & 0x3F;
        var compressionMethodIndex = compressionSlot == 0 ? 0u : compressionSlot - 1;
        var encrypted = (bits & (1 << 22)) != 0;
        var compressionBlockCount = (int)((bits >> 6) & 0xFFFF);

        var compressionBlockSize = bits & 0x3F;
        if (compressionBlockSize == 0x3F)
            compressionBlockSize = reader.ReadUInt32();
        else
            compressionBlockSize <<= 11;

        var offset = ReadVarInt(reader, bits, 31);
        var uncompressedSize = ReadVarInt(reader, bits, 30);
        var compressedSize = compressionMethodIndex == 0
            ? uncompressedSize
            : ReadVarInt(reader, bits, 29);

        var serializedEntrySize = PakEntrySerializer.GetSerializedSize(
            version,
            compressionMethodIndex,
            compressionBlockCount);

        var blocks = ReadCompressionBlocks(
            reader,
            version,
            offset,
            compressedSize,
            compressionBlockCount,
            encrypted,
            compressionMethodIndex != 0);

        entry = new PakEntryRecord
        {
            Path = string.Empty,
            Offset = offset,
            Size = compressedSize,
            UncompressedSize = uncompressedSize,
            CompressionMethodIndex = compressionMethodIndex,
            Hash = [],
            Flags = (byte)(encrypted ? 0x01 : 0),
            CompressionBlockSize = (uint)compressionBlockSize,
            CompressionBlocks = blocks,
            SerializedEntrySize = serializedEntrySize,
        };

        return true;
    }

    public static bool IsDeletedOffset(int encodedOffset) => encodedOffset == DeletedEntrySentinel;

    private static long ReadVarInt(UeBinaryReader reader, uint bits, int bitIndex)
    {
        if ((bits & (1u << bitIndex)) != 0)
            return reader.ReadUInt32();

        return reader.ReadInt64();
    }

    private static IReadOnlyList<PakCompressedBlock> ReadCompressionBlocks(
        UeBinaryReader reader,
        int version,
        long offset,
        long compressedSize,
        int compressionBlockCount,
        bool encrypted,
        bool compressed)
    {
        if (!compressed || compressionBlockCount == 0)
            return [];

        var offsetBase = PakEntrySerializer.GetSerializedSize(version, 1, compressionBlockCount);

        if (compressionBlockCount == 1 && !encrypted)
        {
            return [new PakCompressedBlock(offsetBase, offsetBase + compressedSize)];
        }

        var blocks = new List<PakCompressedBlock>(compressionBlockCount);
        long index = offsetBase;
        for (var i = 0; i < compressionBlockCount; i++)
        {
            var blockSize = reader.ReadUInt32();
            var alignedSize = encrypted ? Align16(blockSize) : blockSize;
            blocks.Add(new PakCompressedBlock(index, index + blockSize));
            index += alignedSize;
        }

        return blocks;
    }

    private static long Align16(long value) => (value + 15) & ~15L;
}
