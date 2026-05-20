using CsStratware.Core.Models;
using CsStratware.ModLoader;

namespace CsStratware.Pak;

public sealed class ModPakReference
{
    public required string ModId { get; init; }
    public required string ModRoot { get; init; }
    public required string PakPath { get; init; }
}

/// <summary>Find built .pak files under a mods directory.</summary>
public static class ModsPakResolver
{
    public static IReadOnlyList<ModPakReference> ResolveFromModsDirectory(string modsDirectory)
    {
        modsDirectory = Path.GetFullPath(modsDirectory);
        var results = new List<ModPakReference>();
        var seenPaks = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        foreach (var root in ModDiscovery.FindModRoots(modsDirectory))
        {
            var package = ModDiscovery.TryLoadPackageAsync(root).GetAwaiter().GetResult();
            if (package is null)
                continue;

            foreach (var pakPath in EnumerateModPakCandidates(package))
            {
                if (!seenPaks.Add(pakPath))
                    continue;

                results.Add(new ModPakReference
                {
                    ModId = package.Manifest.Id,
                    ModRoot = package.RootPath,
                    PakPath = pakPath,
                });
            }
        }

        return results;
    }

    private static IEnumerable<string> EnumerateModPakCandidates(ModPackage package)
    {
        var modDir = package.RootPath;
        var candidates = new List<string>();

        if (!string.IsNullOrWhiteSpace(package.Manifest.Pak?.Output))
        {
            var output = package.Manifest.Pak.Output;
            if (!Path.IsPathRooted(output))
                output = Path.Combine(modDir, output);
            candidates.Add(Path.GetFullPath(output));
        }

        var defaultPak = Path.Combine(modDir, "dist", $"{SanitizePakName(package.Manifest.Id)}_P.pak");
        candidates.Add(Path.GetFullPath(defaultPak));

        var distDir = Path.Combine(modDir, "dist");
        if (Directory.Exists(distDir))
        {
            foreach (var pak in Directory.EnumerateFiles(distDir, "*.pak"))
                candidates.Add(Path.GetFullPath(pak));
        }

        foreach (var path in candidates.Distinct(StringComparer.OrdinalIgnoreCase))
        {
            if (File.Exists(path))
                yield return path;
        }
    }

    private static string SanitizePakName(string id) =>
        id.Replace('.', '-').Replace(' ', '-');
}
