using System.Text;
using CsStratware.Pak.Models;

namespace CsStratware.Pak;

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
        IReadOnlySet<string>? entryPaths = null)
    {
        var results = new List<PakContentMatch>();
        var extFilter = (extensions is { Count: > 0 } ? extensions : DefaultExtensions)
            .Select(e => e.StartsWith('.') ? e : "." + e)
            .ToHashSet(StringComparer.OrdinalIgnoreCase);
        var utf8 = Encoding.UTF8.GetBytes(needle);
        var utf16 = Encoding.Unicode.GetBytes(needle);

        using var stream = File.OpenRead(archive.FilePath);
        foreach (var entry in archive.Entries.Values.Where(e => !e.IsDeleted))
        {
            if (entryPaths is not null && !entryPaths.Contains(entry.Path))
                continue;
            if (!extFilter.Any(ext => entry.Path.EndsWith(ext, StringComparison.OrdinalIgnoreCase)))
                continue;
            if (entry.UncompressedSize <= 0 || entry.UncompressedSize > maxEntryBytes)
                continue;

            byte[] data;
            try
            {
                data = PakEntryExtractor.ReadEntry(stream, entry, archive.Footer);
            }
            catch
            {
                continue;
            }

            var offset = IndexOf(data, utf8);
            if (offset < 0)
                offset = IndexOf(data, utf16);
            if (offset < 0)
                continue;

            results.Add(new PakContentMatch
            {
                PakPath = archive.FilePath,
                EntryPath = entry.Path,
                Offset = offset,
                UncompressedSize = entry.UncompressedSize,
            });

            if (results.Count >= maxResults)
                break;
        }

        return results;
    }

    public static IReadOnlyList<PakContentMatch> GrepDirectory(
        string pakDirectory,
        string needle,
        int maxResults = 50,
        string searchPattern = "*.pak")
    {
        var results = new List<PakContentMatch>();
        foreach (var pakPath in Directory.EnumerateFiles(pakDirectory, searchPattern))
        {
            var archive = PakArchiveReader.Open(pakPath);
            foreach (var match in GrepFile(archive, needle, maxResults - results.Count))
            {
                results.Add(match);
                if (results.Count >= maxResults)
                    return results;
            }
        }

        return results;
    }

    private static int IndexOf(ReadOnlySpan<byte> haystack, ReadOnlySpan<byte> needle)
    {
        if (needle.Length == 0 || haystack.Length < needle.Length)
            return -1;

        for (var i = 0; i <= haystack.Length - needle.Length; i++)
        {
            if (haystack.Slice(i, needle.Length).SequenceEqual(needle))
                return i;
        }

        return -1;
    }
}
