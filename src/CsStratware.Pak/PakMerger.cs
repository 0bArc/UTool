using System.Text;
using CsStratware.ModLoader.Merge;
using CsStratware.Pak.Models;

namespace CsStratware.Pak;

/// <summary>Merge multiple .pak files; optional UE JSON row union for colliding assets.</summary>
public static class PakMerger
{
    public static PakBuildResult Merge(
        IReadOnlyList<string> pakPaths,
        string outputPakPath,
        PakMergeOptions? options = null)
    {
        if (pakPaths.Count == 0)
            throw new ArgumentException("At least one pak path is required.", nameof(pakPaths));

        options ??= new PakMergeOptions();
        var openOptions = options.PakOpenOptions;
        var tempDir = Path.Combine(Path.GetTempPath(), "csstratware-pak-merge-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(tempDir);

        try
        {
            var files = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            PakArchive? mountSource = null;

            foreach (var pakPath in pakPaths)
            {
                var archive = PakArchiveCache.Open(pakPath, openOptions);
                mountSource ??= archive;
                using var stream = File.OpenRead(pakPath);

                foreach (var entry in archive.Entries.Values.Where(e => !e.IsDeleted))
                {
                    var relative = PakEntryPaths.ToRelativePath(entry.Path, archive.MountPoint);
                    var bytes = PakEntryExtractor.ReadEntry(stream, entry, archive.Footer, openOptions?.AesKey);
                    var target = Path.Combine(tempDir, relative.Replace('/', Path.DirectorySeparatorChar));
                    Directory.CreateDirectory(Path.GetDirectoryName(target)!);

                    if (files.TryGetValue(relative, out var existingPath)
                        && options.JsonMerge
                        && IsJsonPath(relative)
                        && TryMergeJsonFiles(existingPath, bytes, target))
                    {
                        files[relative] = target;
                        continue;
                    }

                    File.WriteAllBytes(target, bytes);
                    files[relative] = target;
                }
            }

            mountSource ??= PakArchiveCache.Open(pakPaths[0], openOptions);
            var sources = files
                .OrderBy(kv => kv.Key, StringComparer.OrdinalIgnoreCase)
                .Select(kv => new PakSourceFile(kv.Key, kv.Value));

            var build = options.BuildOptions ?? new PakBuildOptions();
            if (string.IsNullOrWhiteSpace(build.MountPoint) || build.MountPoint == "../../../YourGame/")
            {
                build = new PakBuildOptions
                {
                    MountPoint = mountSource.MountPoint,
                    PakVersion = build.PakVersion,
                    Compression = build.Compression,
                };
            }

            return PakBuilder.Build(sources, outputPakPath, build.MountPoint, build);
        }
        finally
        {
            try { Directory.Delete(tempDir, recursive: true); } catch { /* best effort */ }
        }
    }

    private static bool IsJsonPath(string relativePath) =>
        relativePath.EndsWith(".json", StringComparison.OrdinalIgnoreCase);

    private static bool TryMergeJsonFiles(string existingPath, byte[] overlayBytes, string outputPath)
    {
        try
        {
            var baseText = File.ReadAllText(existingPath, Encoding.UTF8);
            var overlayText = Encoding.UTF8.GetString(overlayBytes);
            if (string.Equals(baseText, overlayText, StringComparison.Ordinal))
            {
                File.WriteAllBytes(outputPath, overlayBytes);
                return true;
            }

            var merged = UeDataTableMerger.MergeToJson(
                baseText,
                overlayText,
                new UeDataTableMergeOptions
                {
                    AssetLabel = Path.GetFileName(outputPath),
                });
            File.WriteAllText(outputPath, merged, Encoding.UTF8);
            return true;
        }
        catch
        {
            return false;
        }
    }
}
