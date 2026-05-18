using System.Collections.Concurrent;
using CsStratware.Infrastructure.Caching;
using CsStratware.Pak.Models;

namespace CsStratware.Pak;

public static class PakArchiveCache
{
    private sealed record CacheEntry(FileIdentity Identity, PakArchive Archive);

    private static readonly ConcurrentDictionary<string, CacheEntry> Entries = new(StringComparer.OrdinalIgnoreCase);

    public static PakArchive Open(string pakPath, PakOpenOptions? options = null)
    {
        pakPath = Path.GetFullPath(pakPath);
        if (!File.Exists(pakPath))
            throw new FileNotFoundException("Pak not found.", pakPath);

        var identity = FileIdentity.FromPath(pakPath);
        if (options?.AesKey is null
            && Entries.TryGetValue(pakPath, out var cached)
            && cached.Identity.CacheKey == identity.CacheKey)
        {
            return cached.Archive;
        }

        var archive = PakArchiveReader.Open(pakPath, options ?? null);
        if (options?.AesKey is null)
            Entries[pakPath] = new CacheEntry(identity, archive);
        return archive;
    }

    public static void Invalidate(string? pakPath = null)
    {
        if (pakPath is null)
        {
            Entries.Clear();
            return;
        }

        Entries.TryRemove(Path.GetFullPath(pakPath), out _);
    }
}
