using CsStratware.Infrastructure.Caching;
using CsStratware.Pak.Models;

namespace CsStratware.Pak;

public static class PakOverlapChecker
{
    public static PakOverlapReport Analyze(
        IReadOnlyList<string> pakPaths,
        PakOpenOptions? openOptions = null)
    {
        if (pakPaths.Count == 0)
            throw new ArgumentException("At least one pak path is required.", nameof(pakPaths));

        var mergeMount = pakPaths.Count > 1
            ? PakEntryPaths.CommonMountPoint(
                pakPaths.Select(p => PakArchiveCache.Open(p, openOptions).MountPoint))
            : null;
        mergeMount = string.IsNullOrWhiteSpace(mergeMount) ? null : PakEntryPaths.NormalizeMountPoint(mergeMount);

        var byRelative = new Dictionary<string, List<(string PakPath, string EntryPath, long Size)>>(StringComparer.OrdinalIgnoreCase);

        foreach (var pakPath in pakPaths)
        {
            var archive = PakArchiveCache.Open(pakPath, openOptions);
            foreach (var entry in archive.Entries.Values.Where(e => !e.IsDeleted))
            {
                var relative = mergeMount is null
                    ? PakEntryPaths.ToRelativePath(entry.Path, archive.MountPoint)
                    : PakEntryPaths.ToRelativePath(entry.Path, mergeMount);
                if (!byRelative.TryGetValue(relative, out var sources))
                {
                    sources = [];
                    byRelative[relative] = sources;
                }

                sources.Add((pakPath, entry.Path, entry.UncompressedSize));
            }
        }

        var conflicts = new List<PakOverlapConflict>();
        foreach (var (relative, rawSources) in byRelative)
        {
            if (rawSources.Count < 2)
                continue;

            var sources = HashSources(rawSources, openOptions, mergeMount);
            var hashes = sources.Select(s => s.ContentHash).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
            conflicts.Add(new PakOverlapConflict
            {
                RelativePath = relative,
                Sources = sources,
                IdenticalContent = hashes.Count <= 1,
            });
        }

        conflicts.Sort((a, b) => string.Compare(a.RelativePath, b.RelativePath, StringComparison.OrdinalIgnoreCase));

        return new PakOverlapReport
        {
            PakPaths = pakPaths,
            Conflicts = conflicts,
            DistinctPaths = byRelative.Count,
        };
    }

    private static List<PakOverlapSource> HashSources(
        IReadOnlyList<(string PakPath, string EntryPath, long Size)> rawSources,
        PakOpenOptions? openOptions,
        string? mergeMount)
    {
        var result = new List<PakOverlapSource>(rawSources.Count);
        foreach (var pakGroup in rawSources.GroupBy(s => s.PakPath, StringComparer.OrdinalIgnoreCase))
        {
            var archive = PakArchiveCache.Open(pakGroup.Key, openOptions);
            using var stream = File.OpenRead(pakGroup.Key);
            foreach (var (pakPath, entryPath, size) in pakGroup)
            {
                if (!archive.Entries.TryGetValue(entryPath, out var entry))
                    continue;

                var bytes = PakEntryExtractor.ReadEntry(stream, entry, archive.Footer, openOptions?.AesKey);
                var relativeMount = mergeMount ?? PakEntryPaths.NormalizeMountPoint(archive.MountPoint);
                var relative = PakEntryPaths.ToRelativePath(entry.Path, relativeMount);
                result.Add(new PakOverlapSource
                {
                    PakPath = pakPath,
                    EntryPath = entryPath,
                    RelativePath = relative,
                    UncompressedSize = size,
                    ContentHash = ContentHasher.HashBytes(bytes),
                });
            }
        }

        return result;
    }
}
