using UTool.Core.Models;

namespace UTool.ModLoader;

public static class ModDiscovery
{
    public static IEnumerable<string> FindModRoots(string modsDirectory)
    {
        if (!Directory.Exists(modsDirectory))
            yield break;

        if (File.Exists(Path.Combine(modsDirectory, ModManifestReader.ManifestFileName)))
        {
            yield return modsDirectory;
            yield break;
        }

        foreach (var dir in Directory.EnumerateDirectories(modsDirectory))
        {
            if (File.Exists(Path.Combine(dir, ModManifestReader.ManifestFileName)))
                yield return dir;
        }
    }

    public static async Task<ModPackage?> TryLoadPackageAsync(
        string modRoot,
        CancellationToken cancellationToken = default)
    {
        var manifestPath = Path.Combine(modRoot, ModManifestReader.ManifestFileName);
        if (!File.Exists(manifestPath))
            return null;

        var manifest = await ModManifestReader.ReadAsync(manifestPath, cancellationToken);
        return new ModPackage
        {
            RootPath = Path.GetFullPath(modRoot),
            Manifest = manifest,
            ManifestPath = manifestPath,
        };
    }
}
