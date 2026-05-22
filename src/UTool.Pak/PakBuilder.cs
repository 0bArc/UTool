using System.Security.Cryptography;
using UTool.Pak.IO;
using UTool.Pak.Models;

namespace UTool.Pak;

public static class PakBuilder
{
    public static PakBuildResult BuildFromDirectory(
        string contentDirectory,
        string outputPakPath,
        PakBuildOptions? options = null)
    {
        options ??= new PakBuildOptions();
        if (!Directory.Exists(contentDirectory))
            throw new DirectoryNotFoundException($"Content directory not found: {contentDirectory}");

        var mountPoint = NormalizeMountPoint(options.MountPoint);
        var files = Directory
            .EnumerateFiles(contentDirectory, "*", SearchOption.AllDirectories)
            .Select(path => new PakSourceFile(
                Path: ToPakRelativePath(contentDirectory, path),
                FullPath: path))
            .OrderBy(f => f.Path, StringComparer.OrdinalIgnoreCase)
            .ToList();

        return Build(files, outputPakPath, mountPoint, options);
    }

    public static PakBuildResult Build(
        IEnumerable<PakSourceFile> files,
        string outputPakPath,
        string mountPoint,
        PakBuildOptions? options = null)
    {
        options ??= new PakBuildOptions();
        mountPoint = NormalizeMountPoint(mountPoint);

        if (options.Compression != PakCompressionMethod.None)
            throw new NotSupportedException("Only uncompressed pak output is supported in this build.");

        var sourceFiles = files.ToList();
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(outputPakPath))!);
        // #region agent log
        DebugLog("PakBuilder.cs:Build", "build input summary", "H4", new
        {
            output = Path.GetFileName(outputPakPath),
            mountPoint,
            sourceCount = sourceFiles.Count,
            totalInputBytes = sourceFiles.Sum(f => new FileInfo(f.FullPath).Length),
            sample = sourceFiles
                .Take(12)
                .Select(f => new { f.Path, bytes = new FileInfo(f.FullPath).Length })
                .ToArray(),
        });
        // #endregion

        using var stream = File.Create(outputPakPath);
        var staged = new List<StagedEntry>(sourceFiles.Count);

        foreach (var file in sourceFiles)
        {
            var bytes = File.ReadAllBytes(file.FullPath);
            var hash = SHA1.HashData(bytes);
            var offset = stream.Position;

            var record = new PakEntryRecord
            {
                Path = PakArchiveReader.CombineMountPath(mountPoint, file.Path),
                Offset = offset,
                Size = bytes.Length,
                UncompressedSize = bytes.Length,
                CompressionMethodIndex = 0,
                Hash = hash,
                Flags = 0,
                CompressionBlockSize = 0,
                CompressionBlocks = [],
                SerializedEntrySize = 0,
            };

            using (var writer = new UeBinaryWriter(stream))
                PakEntrySerializer.Write(writer, record);

            var serializedEntrySize = (int)(stream.Position - offset);
            stream.Write(bytes, 0, bytes.Length);

            staged.Add(new StagedEntry(file.Path, new PakEntryRecord
            {
                Path = record.Path,
                Offset = record.Offset,
                Size = record.Size,
                UncompressedSize = record.UncompressedSize,
                CompressionMethodIndex = record.CompressionMethodIndex,
                Hash = record.Hash,
                Flags = record.Flags,
                CompressionBlockSize = record.CompressionBlockSize,
                CompressionBlocks = record.CompressionBlocks,
                SerializedEntrySize = serializedEntrySize,
            }));
        }

        var indexOffset = stream.Position;
        byte[] indexBytes;
        using (var indexStream = new MemoryStream())
        using (var indexWriter = new UeBinaryWriter(indexStream))
        {
            indexWriter.WriteFString(mountPoint);
            indexWriter.Write(staged.Count);

            foreach (var entry in staged)
            {
                indexWriter.WriteFString(entry.RelativePath);
                PakEntrySerializer.Write(indexWriter, entry.Record);
            }

            indexBytes = indexStream.ToArray();
        }

        stream.Write(indexBytes, 0, indexBytes.Length);
        var indexHash = SHA1.HashData(indexBytes);

        using (var footerWriter = new UeBinaryWriter(stream))
        {
            footerWriter.Write(Guid.Empty.ToByteArray());
            footerWriter.Write((byte)0);
            footerWriter.Write(PakFormat.Magic);
            footerWriter.Write(options.PakVersion);
            footerWriter.Write(indexOffset);
            footerWriter.Write((long)indexBytes.Length);
            footerWriter.Write(indexHash);

            var methodBuffer = new byte[PakFormat.CompressionMethodNameLength * PakFormat.MaxCompressionMethods];
            WriteMethodName(methodBuffer, 0, "None");
            WriteMethodName(methodBuffer, 1, "Zlib");
            footerWriter.Write(methodBuffer);
        }

        return new PakBuildResult
        {
            OutputPath = Path.GetFullPath(outputPakPath),
            FileCount = staged.Count,
            TotalBytes = staged.Sum(s => s.Record.UncompressedSize),
        };
    }

    private static void WriteMethodName(byte[] buffer, int index, string name)
    {
        var offset = index * PakFormat.CompressionMethodNameLength;
        var bytes = System.Text.Encoding.ASCII.GetBytes(name);
        Array.Copy(bytes, 0, buffer, offset, Math.Min(bytes.Length, PakFormat.CompressionMethodNameLength - 1));
    }

    private static string NormalizeMountPoint(string mountPoint)
    {
        mountPoint = mountPoint.Replace('\\', '/');
        if (!mountPoint.EndsWith('/'))
            mountPoint += '/';
        return mountPoint;
    }

    private static string ToPakRelativePath(string root, string fullPath)
    {
        var relative = Path.GetRelativePath(root, fullPath).Replace('\\', '/');
        return relative;
    }

    private static void DebugLog(string location, string message, string hypothesisId, object data)
    {
        try
        {
            var payload = new
            {
                sessionId = "1ee33a",
                runId = "pre-fix",
                hypothesisId,
                location,
                message,
                data,
                timestamp = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
            };
            File.AppendAllText(@"f:\Data\personal\c#\utool\debug-1ee33a.log", System.Text.Json.JsonSerializer.Serialize(payload) + Environment.NewLine);
        }
        catch
        {
            // Debug logging must not affect pak build behavior.
        }
    }

    private sealed record StagedEntry(string RelativePath, PakEntryRecord Record);
}

public readonly record struct PakSourceFile(string Path, string FullPath);
