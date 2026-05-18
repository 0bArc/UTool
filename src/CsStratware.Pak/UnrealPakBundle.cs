using System.IO.Compression;

namespace CsStratware.Pak;

public static class UnrealPakBundle
{
    public const string AssetsFolderName = "assets";
    public const string ZipFileName = "UnrealPak.zip";
    public const string LegacyZipRelative = ".required/UnrealPak.zip";

    public static string? TryFindRepoRoot(string? configDirectory = null)
    {
        foreach (var start in CandidateRoots(configDirectory))
        {
            var dir = start;
            for (var i = 0; i < 14 && !string.IsNullOrEmpty(dir); i++)
            {
                if (File.Exists(Path.Combine(dir, "csStratware.sln")))
                    return Path.GetFullPath(dir);

                if (TryResolveZipPath(dir) is not null)
                    return Path.GetFullPath(dir);

                dir = Directory.GetParent(dir)?.FullName;
            }
        }

        return null;
    }

    public static string? TryResolveZipPath(string? repoRoot)
    {
        if (string.IsNullOrWhiteSpace(repoRoot))
            return null;

        var primary = Path.Combine(repoRoot, AssetsFolderName, ZipFileName);
        if (File.Exists(primary))
            return primary;

        var legacy = Path.Combine(repoRoot, LegacyZipRelative.Replace('/', Path.DirectorySeparatorChar));
        return File.Exists(legacy) ? legacy : null;
    }

    public static string? TryResolveStoreRoot(string? configDirectory = null)
    {
        var repo = TryFindRepoRoot(configDirectory);
        return repo is null
            ? null
            : Path.Combine(repo, AssetsFolderName, UnrealPakToolchain.BundleFolderName);
    }

    public static bool TryEnsureExtracted(string? configDirectory = null, bool force = false)
    {
        var repo = TryFindRepoRoot(configDirectory);
        if (repo is null)
            return false;

        var zipPath = TryResolveZipPath(repo);
        if (zipPath is null)
            return false;

        var storeRoot = Path.Combine(repo, AssetsFolderName, UnrealPakToolchain.BundleFolderName);
        var exe = Path.Combine(storeRoot, UnrealPakToolchain.RelativeExecutable);
        if (!force && File.Exists(exe) && File.GetLastWriteTimeUtc(exe) >= File.GetLastWriteTimeUtc(zipPath))
            return true;

        ExtractZip(zipPath, Path.Combine(repo, AssetsFolderName));
        return File.Exists(exe);
    }

    private static void ExtractZip(string zipPath, string extractRoot)
    {
        Directory.CreateDirectory(extractRoot);
        using var archive = ZipFile.OpenRead(zipPath);
        foreach (var entry in archive.Entries)
        {
            if (string.IsNullOrEmpty(entry.Name))
                continue;

            var target = Path.Combine(extractRoot, entry.FullName.Replace('/', Path.DirectorySeparatorChar));
            var parent = Path.GetDirectoryName(target);
            if (!string.IsNullOrEmpty(parent))
                Directory.CreateDirectory(parent);

            entry.ExtractToFile(target, overwrite: true);
        }
    }

    private static IEnumerable<string> CandidateRoots(string? configDirectory)
    {
        if (!string.IsNullOrWhiteSpace(configDirectory))
            yield return configDirectory;

        var processDir = GetProcessDirectory();
        if (!string.IsNullOrWhiteSpace(processDir))
            yield return processDir;

        yield return Directory.GetCurrentDirectory();

        var fromEnv = Environment.GetEnvironmentVariable("CSSTRATWARE_ROOT");
        if (!string.IsNullOrWhiteSpace(fromEnv))
            yield return fromEnv;
    }

    private static string? GetProcessDirectory()
    {
        var processPath = Environment.ProcessPath;
        return string.IsNullOrWhiteSpace(processPath)
            ? null
            : Path.GetDirectoryName(processPath);
    }
}
