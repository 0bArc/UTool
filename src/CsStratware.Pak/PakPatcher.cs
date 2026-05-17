using CsStratware.Pak.Models;

namespace CsStratware.Pak;

/// <summary>Merge a base pak with overlay files into a new pak.</summary>
public static class PakPatcher
{
    public static PakBuildResult Patch(
        string basePakPath,
        string overlayDirectory,
        string outputPakPath,
        PakBuildOptions? options = null)
    {
        var archive = PakArchiveReader.Open(basePakPath);
        using var baseStream = File.OpenRead(basePakPath);
        var tempDir = Path.Combine(Path.GetTempPath(), "csstratware-pak-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(tempDir);

        try
        {
            var extracted = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);

            foreach (var entry in archive.Entries.Values.Where(e => !e.IsDeleted))
            {
                var relative = ToRelativePath(entry.Path, archive.MountPoint);
                var target = Path.Combine(tempDir, relative.Replace('/', Path.DirectorySeparatorChar));
                Directory.CreateDirectory(Path.GetDirectoryName(target)!);
                var bytes = PakEntryExtractor.ReadEntry(baseStream, entry, archive.Footer);
                File.WriteAllBytes(target, bytes);
                extracted[relative] = target;
            }

            if (Directory.Exists(overlayDirectory))
            {
                foreach (var overlayFile in Directory.EnumerateFiles(overlayDirectory, "*", SearchOption.AllDirectories))
                {
                    var relative = Path.GetRelativePath(overlayDirectory, overlayFile).Replace('\\', '/');
                    var target = Path.Combine(tempDir, relative.Replace('/', Path.DirectorySeparatorChar));
                    Directory.CreateDirectory(Path.GetDirectoryName(target)!);
                    File.Copy(overlayFile, target, overwrite: true);
                    extracted[relative] = target;
                }
            }

            var sources = extracted
                .OrderBy(kv => kv.Key, StringComparer.OrdinalIgnoreCase)
                .Select(kv => new PakSourceFile(kv.Key, kv.Value));

            options ??= new PakBuildOptions();
            if (string.IsNullOrWhiteSpace(options.MountPoint))
            {
                options = new PakBuildOptions
                {
                    MountPoint = archive.MountPoint,
                    PakVersion = options.PakVersion,
                    Compression = options.Compression,
                };
            }

            return PakBuilder.Build(sources, outputPakPath, options.MountPoint, options);
        }
        finally
        {
            try
            {
                Directory.Delete(tempDir, recursive: true);
            }
            catch
            {
                // Best-effort temp cleanup.
            }
        }
    }

    private static string ToRelativePath(string entryPath, string mountPoint)
    {
        if (entryPath.StartsWith(mountPoint, StringComparison.OrdinalIgnoreCase))
            return entryPath[mountPoint.Length..].TrimStart('/', '\\');

        return entryPath.TrimStart('/', '\\');
    }
}
