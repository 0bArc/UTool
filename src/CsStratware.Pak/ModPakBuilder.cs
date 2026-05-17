using CsStratware.Core.Models;

namespace CsStratware.Pak;

public static class ModPakBuilder
{
    public static PakBuildResult BuildModPak(
        ModPackage mod,
        string outputPakPath,
        PakBuildOptions? options = null)
    {
        options ??= new PakBuildOptions();
        if (mod.Manifest.Target?.GameId is { Length: > 0 } gameId
            && options.MountPoint == "../../../YourGame/")
        {
            options = new PakBuildOptions
            {
                MountPoint = $"../../../{gameId}/",
                PakVersion = options.PakVersion,
                Compression = options.Compression,
            };
        }

        var roots = mod.Manifest.ContentRoots
            .Select(root => Path.Combine(mod.RootPath, root))
            .Where(Directory.Exists)
            .ToList();

        if (roots.Count == 0)
            throw new InvalidOperationException($"Mod '{mod.Manifest.Id}' has no content roots on disk.");

        if (roots.Count == 1)
            return PakBuilder.BuildFromDirectory(roots[0], outputPakPath, options);

        var tempDir = Path.Combine(Path.GetTempPath(), "csstratware-mod-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(tempDir);

        try
        {
            foreach (var root in roots)
            {
                foreach (var file in Directory.EnumerateFiles(root, "*", SearchOption.AllDirectories))
                {
                    var relative = Path.GetRelativePath(root, file);
                    var target = Path.Combine(tempDir, relative);
                    Directory.CreateDirectory(Path.GetDirectoryName(target)!);
                    File.Copy(file, target, overwrite: true);
                }
            }

            return PakBuilder.BuildFromDirectory(tempDir, outputPakPath, options);
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
}
