using UTool.Core.Models;
using UTool.Infrastructure.IO;
using UTool.Infrastructure.Logging;

namespace UTool.Pak;

public static class ModBuildContent
{
    public static string MergeForPack(ModPackage mod, string preparedDir)
    {
        var roots = mod.Manifest.ContentRoots
            .Select(r => Path.Combine(mod.RootPath, r))
            .Where(Directory.Exists)
            .ToList();

        if (roots.Count == 0 && !Directory.Exists(preparedDir))
            throw new InvalidOperationException($"No packable content for mod '{mod.Manifest.Id}'.");

        if (roots.Count == 0)
            return preparedDir;

        var hasPrepared = Directory.Exists(preparedDir)
            && Directory.EnumerateFiles(preparedDir, "*", SearchOption.AllDirectories).Any();

        if (!hasPrepared)
            return roots.Count == 1 ? roots[0] : MergeRoots(mod, roots);

        var merged = Path.Combine(mod.RootPath, ".cache", "pack-content");
        if (Directory.Exists(merged))
        {
            try { Directory.Delete(merged, recursive: true); } catch { /* ignore */ }
        }

        Directory.CreateDirectory(merged);
        CopyTree(preparedDir, merged);

        foreach (var root in roots)
            CopyTree(root, merged);

        UToolLog.Info("merged pack content", new { mod = mod.Manifest.Id, merged });
        return merged;
    }

    private static string MergeRoots(ModPackage mod, IReadOnlyList<string> roots)
    {
        var merged = Path.Combine(mod.RootPath, ".cache", "pack-content");
        if (Directory.Exists(merged))
        {
            try { Directory.Delete(merged, recursive: true); } catch { /* ignore */ }
        }

        Directory.CreateDirectory(merged);
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var root in roots)
        {
            foreach (var file in Directory.EnumerateFiles(root, "*", SearchOption.AllDirectories))
            {
                var relative = Path.GetRelativePath(root, file).Replace('\\', '/');
                if (!seen.Add(relative))
                    continue;

                var target = Path.Combine(merged, relative.Replace('/', Path.DirectorySeparatorChar));
                Directory.CreateDirectory(Path.GetDirectoryName(target)!);
                StreamingFileOps.TryLinkOrCopy(file, target);
            }
        }

        return merged;
    }

    private static void CopyTree(string source, string target)
    {
        foreach (var file in Directory.EnumerateFiles(source, "*", SearchOption.AllDirectories))
        {
            var relative = Path.GetRelativePath(source, file).Replace('\\', '/');
            var dest = Path.Combine(target, relative.Replace('/', Path.DirectorySeparatorChar));
            Directory.CreateDirectory(Path.GetDirectoryName(dest)!);
            StreamingFileOps.TryLinkOrCopy(file, dest);
        }
    }
}
