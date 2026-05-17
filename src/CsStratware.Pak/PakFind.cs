using System.Text;
using CsStratware.Pak.Models;

namespace CsStratware.Pak;

public enum PakFindHitKind
{
    Path,
    Content,
    Disk,
}

public sealed class PakFindHit
{
    public required PakFindHitKind Kind { get; init; }
    public string? PakPath { get; init; }
    public string? EntryPath { get; init; }
    public string? FilePath { get; init; }
    public int? Offset { get; init; }
    public long? Size { get; init; }
}

public sealed class PakFindOptions
{
    public int MaxResults { get; init; } = 30;
    public bool PathOnly { get; init; }
    public bool GrepContent { get; init; }
    public string? ExtractedDir { get; init; }
    public IReadOnlyList<string> Extensions { get; init; } =
        [".json", ".uasset", ".uexp", ".ini"];
    public long MaxGrepEntryBytes { get; init; } = 16 * 1024 * 1024;
}

public static class PakFind
{
    public static IReadOnlyList<PakFindHit> Find(
        string target,
        string needle,
        PakFindOptions? options = null)
    {
        options ??= new PakFindOptions();
        var hits = new List<PakFindHit>();

        if (Directory.Exists(target))
            FindInPakDirectory(target, needle, options, hits);
        else if (File.Exists(target) && target.EndsWith(".pak", StringComparison.OrdinalIgnoreCase))
            FindInArchive(PakArchiveReader.Open(target), needle, options, hits);
        else if (Directory.Exists(options.ExtractedDir))
            FindOnDisk(options.ExtractedDir, needle, options, hits);

        if (hits.Count < options.MaxResults
            && !string.IsNullOrWhiteSpace(options.ExtractedDir)
            && Directory.Exists(options.ExtractedDir))
        {
            FindOnDisk(options.ExtractedDir, needle, options, hits);
        }

        return hits;
    }

    private static void FindInPakDirectory(
        string pakDirectory,
        string needle,
        PakFindOptions options,
        List<PakFindHit> hits)
    {
        foreach (var pakPath in EnumerateSearchPaks(pakDirectory))
        {
            var archive = PakArchiveReader.Open(pakPath);
            FindInArchive(archive, needle, options, hits);
            if (hits.Count >= options.MaxResults)
                return;
        }
    }

    private static IEnumerable<string> EnumerateSearchPaks(string pakDirectory)
    {
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

    private static void FindInArchive(
        PakArchive archive,
        string needle,
        PakFindOptions options,
        List<PakFindHit> hits)
    {
        var pathMatches = PakArchiveSearch.SearchFile(archive, new PakSearchOptions
        {
            Pattern = needle,
            MaxResults = options.MaxResults - hits.Count,
        });

        foreach (var match in pathMatches)
        {
            hits.Add(new PakFindHit
            {
                Kind = PakFindHitKind.Path,
                PakPath = match.PakPath,
                EntryPath = match.Entry.Path,
                Size = match.Entry.UncompressedSize,
            });
        }

        if (options.PathOnly || !options.GrepContent || hits.Count >= options.MaxResults)
            return;

        if (pathMatches.Count == 0)
            return;

        var pathFilter = pathMatches.Select(m => m.Entry.Path).ToHashSet(StringComparer.OrdinalIgnoreCase);

        foreach (var match in PakContentSearch.GrepFile(
                     archive,
                     needle,
                     options.MaxResults - hits.Count,
                     options.MaxGrepEntryBytes,
                     options.Extensions,
                     pathFilter))
        {
            hits.Add(new PakFindHit
            {
                Kind = PakFindHitKind.Content,
                PakPath = match.PakPath,
                EntryPath = match.EntryPath,
                Offset = match.Offset,
                Size = match.UncompressedSize,
            });
        }
    }

    private static void FindOnDisk(
        string directory,
        string needle,
        PakFindOptions options,
        List<PakFindHit> hits)
    {
        var exts = options.Extensions
            .Select(e => e.StartsWith('.') ? e : "." + e)
            .ToHashSet(StringComparer.OrdinalIgnoreCase);

        foreach (var file in Directory.EnumerateFiles(directory, "*", SearchOption.AllDirectories))
        {
            var ext = Path.GetExtension(file);
            if (exts.Count > 0 && !exts.Contains(ext))
                continue;

            if (!Path.GetFileName(file).Contains(needle, StringComparison.OrdinalIgnoreCase)
                && !ContainsNeedle(file, needle))
                continue;

            hits.Add(new PakFindHit
            {
                Kind = PakFindHitKind.Disk,
                FilePath = file,
            });

            if (hits.Count >= options.MaxResults)
                return;
        }
    }

    private static bool ContainsNeedle(string filePath, string needle)
    {
        using var stream = File.OpenRead(filePath);
        var buf = new byte[Math.Min(512 * 1024, stream.Length)];
        var read = stream.Read(buf, 0, buf.Length);
        var text = Encoding.UTF8.GetString(buf, 0, read);
        return text.Contains(needle, StringComparison.OrdinalIgnoreCase);
    }
}
