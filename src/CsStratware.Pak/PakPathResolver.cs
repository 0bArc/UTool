namespace CsStratware.Pak;

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
