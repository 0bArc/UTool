using CsStratware.Pak.Models;

namespace CsStratware.Pak;

public sealed class PakSearchOptions
{
    public string? Pattern { get; init; }
    public IReadOnlyList<string> Extensions { get; init; } = [];
    public bool IgnoreCase { get; init; } = true;
    public int MaxResults { get; init; } = 500;
}

public sealed class PakSearchMatch
{
    public required string PakPath { get; init; }
    public required PakEntryRecord Entry { get; init; }
}

public static class PakArchiveSearch
{
    public static IReadOnlyList<PakSearchMatch> SearchFile(PakArchive archive, PakSearchOptions options)
    {
        var pattern = options.Pattern;
        var comparison = options.IgnoreCase ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal;
        var extensions = options.Extensions
            .Select(e => e.StartsWith('.') ? e : "." + e)
            .ToHashSet(StringComparer.OrdinalIgnoreCase);

        var results = new List<PakSearchMatch>();
        foreach (var entry in archive.Entries.Values.Where(e => !e.IsDeleted))
        {
            if (extensions.Count > 0 && !extensions.Any(ext => entry.Path.EndsWith(ext, comparison)))
                continue;

            if (!string.IsNullOrEmpty(pattern)
                && entry.Path.IndexOf(pattern, comparison) < 0)
                continue;

            results.Add(new PakSearchMatch { PakPath = archive.FilePath, Entry = entry });
            if (results.Count >= options.MaxResults)
                break;
        }

        return results;
    }

    public static IReadOnlyList<PakSearchMatch> SearchDirectory(
        string pakDirectory,
        PakSearchOptions options,
        string searchPattern = "*.pak")
    {
        var results = new List<PakSearchMatch>();
        if (!Directory.Exists(pakDirectory))
            return results;

        foreach (var pakPath in Directory.EnumerateFiles(pakDirectory, searchPattern))
        {
            var archive = PakArchiveCache.Open(pakPath);
            var remaining = options.MaxResults - results.Count;
            foreach (var match in SearchFile(archive, new PakSearchOptions
            {
                Pattern = options.Pattern,
                Extensions = options.Extensions,
                IgnoreCase = options.IgnoreCase,
                MaxResults = remaining,
            }))
            {
                results.Add(match);
                if (results.Count >= options.MaxResults)
                    return results;
            }
        }

        return results;
    }
}
