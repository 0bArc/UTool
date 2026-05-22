namespace UTool.Pak;

/// <summary>Resolve a .pak file, a paks directory, or enumerate game pak sets.</summary>
public static class PakPathResolver
{
    public static bool IsPakFile(string path) =>
        path.EndsWith(".pak", StringComparison.OrdinalIgnoreCase) && File.Exists(path);

    public static bool IsPakDirectory(string path) =>
        Directory.Exists(path);

    public static IReadOnlyList<string> Resolve(string target)
    {
        target = Path.GetFullPath(target);
        if (IsPakFile(target))
            return [target];

        if (IsPakDirectory(target))
            return EnumeratePakFiles(target).ToList();

        throw new FileNotFoundException(
            $"Pak source not found or not a .pak file: {target}");
    }

    /// <summary>Resolve merge inputs: mod roots (mod.json + dist/*.pak) before flat directory scan.</summary>
    public static IReadOnlyList<string> ResolveForMerge(string target, string? excludeOutputPakPath = null)
    {
        target = Path.GetFullPath(target);
        if (IsPakFile(target))
            return FilterMergeInputs([target], excludeOutputPakPath);

        if (IsPakDirectory(target))
        {
            var modPaks = ModsPakResolver.ResolveFromModsDirectory(target);
            if (modPaks.Count > 0)
                return FilterMergeInputs(modPaks.Select(m => m.PakPath), excludeOutputPakPath);

            return FilterMergeInputs(EnumeratePakFiles(target), excludeOutputPakPath);
        }

        throw new FileNotFoundException(
            $"Pak source not found or not a .pak file: {target}");
    }

    public static IReadOnlyList<string> FilterMergeInputs(
        IEnumerable<string> pakPaths,
        string? excludeOutputPakPath = null)
    {
        var exclude = string.IsNullOrWhiteSpace(excludeOutputPakPath)
            ? null
            : Path.GetFullPath(excludeOutputPakPath);

        return pakPaths
            .Select(Path.GetFullPath)
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .Where(path =>
            {
                if (exclude is not null && string.Equals(path, exclude, StringComparison.OrdinalIgnoreCase))
                    return false;

                if (IsMergeOutputArtifact(path))
                    return false;

                return IsReadablePak(path);
            })
            .ToList();
    }

    /// <summary>Skip prior merged.pak in directory scans so it cannot shrink the next merge.</summary>
    private static bool IsMergeOutputArtifact(string path) =>
        string.Equals(Path.GetFileName(path), "merged.pak", StringComparison.OrdinalIgnoreCase);

    private static bool IsReadablePak(string path)
    {
        if (!File.Exists(path))
            return false;

        try
        {
            var archive = PakArchiveCache.Open(path);
            return archive.Entries.Values.Any(e => !e.IsDeleted);
        }
        catch
        {
            return false;
        }
    }

    /// <summary>All *.pak under <paramref name="pakDirectory"/> plus ../Data/data.pak when present.</summary>
    public static IEnumerable<string> EnumeratePakFiles(string pakDirectory)
    {
        pakDirectory = Path.GetFullPath(pakDirectory);
        if (!Directory.Exists(pakDirectory))
            yield break;

        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var pakPath in Directory.EnumerateFiles(pakDirectory, "*.pak"))
        {
            if (seen.Add(pakPath))
                yield return pakPath;
        }

        var dataPak = Path.GetFullPath(Path.Combine(pakDirectory, "..", "Data", "data.pak"));
        if (File.Exists(dataPak) && seen.Add(dataPak))
            yield return dataPak;
    }
}
