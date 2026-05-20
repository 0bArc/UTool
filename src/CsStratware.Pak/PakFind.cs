using System.Buffers;
using System.Text;
using CsStratware.Infrastructure.Caching;
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
    /// <summary>When true, skip content grep. Use with --grep for path + content.</summary>
    public bool PathOnly { get; init; }
    public bool GrepContent { get; init; }
    public string? ExtractedDir { get; init; }
    public IReadOnlyList<string> Extensions { get; init; } =
        [".json", ".uasset", ".uexp", ".ini"];
    public long MaxGrepEntryBytes { get; init; } = 16 * 1024 * 1024;
    public PakOpenOptions? PakOpenOptions { get; init; }
}

/// <summary>Search pak paths, optional content grep, and indexed extracted trees.</summary>
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
            FindInArchive(PakArchiveCache.Open(target, options.PakOpenOptions), needle, options, hits);

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
        foreach (var pakPath in PakPathResolver.EnumeratePakFiles(pakDirectory))
        {
            var archive = PakArchiveCache.Open(pakPath, options.PakOpenOptions);
            FindInArchive(archive, needle, options, hits);
            if (hits.Count >= options.MaxResults)
                return;
        }
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

        var wantContent = options.GrepContent && !options.PathOnly;
        if (!wantContent || hits.Count >= options.MaxResults)
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
        var index = AssetIndexCache.ForDirectory(directory);
        index.GetOrBuild();

        var indexed = index.FindByFileName(needle);
        if (indexed is not null && File.Exists(indexed))
        {
            hits.Add(new PakFindHit { Kind = PakFindHitKind.Disk, FilePath = indexed });
            if (hits.Count >= options.MaxResults)
                return;
        }

        var exts = options.Extensions
            .Select(e => e.StartsWith('.') ? e : "." + e)
            .ToHashSet(StringComparer.OrdinalIgnoreCase);

        foreach (var file in Directory.EnumerateFiles(directory, "*", SearchOption.AllDirectories))
        {
            var ext = Path.GetExtension(file);
            if (exts.Count > 0 && !exts.Contains(ext))
                continue;

            if (!Path.GetFileName(file).Contains(needle, StringComparison.OrdinalIgnoreCase)
                && !ContainsNeedleStreaming(file, needle))
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

    private static bool ContainsNeedleStreaming(string filePath, string needle)
    {
        var utf8 = Encoding.UTF8.GetBytes(needle);
        var buf = ArrayPool<byte>.Shared.Rent(512 * 1024 + utf8.Length);
        try
        {
            using var stream = File.OpenRead(filePath);
            var carry = 0;
            int read;
            while ((read = stream.Read(buf, carry, buf.Length - carry)) > 0)
            {
                var window = buf.AsSpan(0, carry + read);
                if (BinarySpanSearch.Contains(window, utf8))
                    return true;

                var overlap = utf8.Length - 1;
                if (overlap > 0 && window.Length > overlap)
                {
                    window.Slice(window.Length - overlap).CopyTo(buf);
                    carry = overlap;
                }
                else
                {
                    carry = 0;
                }
            }
        }
        finally
        {
            ArrayPool<byte>.Shared.Return(buf);
        }

        return false;
    }
}
