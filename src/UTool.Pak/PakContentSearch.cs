using UTool.Infrastructure.Operations;
using UTool.Pak.Models;

namespace UTool.Pak;

public sealed class PakContentMatch
{
    public required string PakPath { get; init; }
    public required string EntryPath { get; init; }
    public required int Offset { get; init; }
    public required long UncompressedSize { get; init; }
}

public static class PakContentSearch
{
    private static readonly string[] DefaultExtensions = [".uasset", ".uexp", ".ini", ".uplugin"];

    public static IReadOnlyList<PakContentMatch> GrepFile(
        PakArchive archive,
        string needle,
        int maxResults = 50,
        long maxEntryBytes = 32 * 1024 * 1024,
        IReadOnlyList<string>? extensions = null,
        IReadOnlySet<string>? entryPaths = null,
        OperationContext? context = null)
    {
        var results = new List<PakContentMatch>();
        var extFilter = (extensions is { Count: > 0 } ? extensions : DefaultExtensions)
            .Select(e => e.StartsWith('.') ? e : "." + e)
            .ToHashSet(StringComparer.OrdinalIgnoreCase);
        var (utf8, utf16) = StreamingPakGrep.NeedleBytes(needle);

        using var stream = File.OpenRead(archive.FilePath);
        foreach (var entry in archive.Entries.Values.Where(e => !e.IsDeleted))
        {
            context?.CancellationToken.ThrowIfCancellationRequested();

            if (entryPaths is not null && !entryPaths.Contains(entry.Path))
                continue;
            if (!extFilter.Any(ext => entry.Path.EndsWith(ext, StringComparison.OrdinalIgnoreCase)))
                continue;
            if (entry.UncompressedSize <= 0 || entry.UncompressedSize > maxEntryBytes)
                continue;

            try
            {
                if (StreamingPakGrep.TrySearchEntry(stream, entry, archive.Footer, utf8, utf16, out var offset))
                {
                    results.Add(new PakContentMatch
                    {
                        PakPath = archive.FilePath,
                        EntryPath = entry.Path,
                        Offset = offset,
                        UncompressedSize = entry.UncompressedSize,
                    });
                }
                else
                {
                    var data = PakEntryExtractor.ReadEntry(stream, entry, archive.Footer);
                    offset = BinarySpanSearch.IndexOf(data, utf8);
                    if (offset < 0)
                        offset = BinarySpanSearch.IndexOf(data, utf16);
                    if (offset < 0)
                        continue;

                    results.Add(new PakContentMatch
                    {
                        PakPath = archive.FilePath,
                        EntryPath = entry.Path,
                        Offset = offset,
                        UncompressedSize = entry.UncompressedSize,
                    });
                }
            }
            catch (NotSupportedException)
            {
                throw;
            }
            catch
            {
                continue;
            }

            if (results.Count >= maxResults)
                break;
        }

        return results;
    }

    public static IReadOnlyList<PakContentMatch> GrepDirectory(
        string pakDirectory,
        string needle,
        int maxResults = 50,
        string searchPattern = "*.pak",
        OperationContext? context = null)
    {
        var results = new List<PakContentMatch>();
        foreach (var pakPath in Directory.EnumerateFiles(pakDirectory, searchPattern))
        {
            context?.CancellationToken.ThrowIfCancellationRequested();
            var archive = PakArchiveCache.Open(pakPath);
            foreach (var match in GrepFile(archive, needle, maxResults - results.Count, context: context))
            {
                results.Add(match);
                if (results.Count >= maxResults)
                    return results;
            }
        }

        return results;
    }
}
