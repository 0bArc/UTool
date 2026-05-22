using UTool.Pak.Models;

namespace UTool.Pak;

public sealed class PakDataPullOptions
{
    public string? Pattern { get; init; }
    public IReadOnlyList<string> Extensions { get; init; } =
        [".json", ".ini", ".csv", ".lua", ".txt", ".xml", ".yaml", ".yml"];
    public int MaxFiles { get; init; }
    public PakOpenOptions? PakOpenOptions { get; init; }
    public UnrealPakOptions? UnrealPakOptions { get; init; }
    public bool UnrealPakFallback { get; init; } = true;
    public Action<string>? Log { get; init; }
}

public sealed class PakDataPullResult
{
    public required string OutputDirectory { get; init; }
    public int Written { get; init; }
    public int SkippedEncrypted { get; init; }
    public int UnrealPakExtracted { get; init; }
    public IReadOnlyList<string> WrittenFiles { get; init; } = [];
}

public static class PakDataPuller
{
    public static IReadOnlyList<PakSearchMatch> List(
        IReadOnlyList<string> pakPaths,
        PakDataPullOptions options)
    {
        var search = ToSearchOptions(options);
        var results = new List<PakSearchMatch>();
        foreach (var pakPath in pakPaths)
        {
            var archive = PakArchiveCache.Open(pakPath, options.PakOpenOptions);
            foreach (var match in PakArchiveSearch.SearchFile(archive, search))
            {
                results.Add(match);
                if (options.MaxFiles > 0 && results.Count >= options.MaxFiles)
                    return results;
            }
        }

        return results;
    }

    public static PakDataPullResult Pull(
        IReadOnlyList<string> pakPaths,
        string outputDirectory,
        PakDataPullOptions? options = null)
    {
        options ??= new PakDataPullOptions();
        outputDirectory = Path.GetFullPath(outputDirectory);
        Directory.CreateDirectory(outputDirectory);

        var aesKey = options.PakOpenOptions?.AesKey;
        var matches = List(pakPaths, options);
        var written = new List<string>();
        var ueQueue = new List<(string PakPath, PakEntryRecord Entry)>();
        var writtenSet = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        foreach (var match in matches)
        {
            var archive = PakArchiveCache.Open(match.PakPath, options.PakOpenOptions);
            var key = NormalizeKey(match.PakPath, match.Entry.Path);
            try
            {
                using var stream = File.OpenRead(match.PakPath);
                var data = PakEntryExtractor.ReadEntry(stream, match.Entry, archive.Footer, aesKey);
                if (!IsUsableTextPayload(data, match.Entry.Path))
                {
                    options.Log?.Invoke($"invalid payload, defer: {match.Entry.Path}");
                    ueQueue.Add((match.PakPath, match.Entry));
                    continue;
                }

                var target = WriteEntry(outputDirectory, match.Entry.Path, archive.MountPoint, data);
                if (!writtenSet.Add(key))
                    continue;

                written.Add(target);
                options.Log?.Invoke($"wrote: {target}");
            }
            catch (NotSupportedException ex)
                when (options.UnrealPakFallback
                      && (ex.Message.Contains("Oodle", StringComparison.OrdinalIgnoreCase)
                          || ex.Message.Contains("Encrypted", StringComparison.OrdinalIgnoreCase)
                          || ex.Message.Contains("Compression method", StringComparison.OrdinalIgnoreCase)))
            {
                ueQueue.Add((match.PakPath, match.Entry));
                options.Log?.Invoke($"defer: {match.Entry.Path} ({ex.Message})");
            }
        }

        var ueCount = 0;
        if (options.UnrealPakFallback && ueQueue.Count > 0)
        {
            ueCount = PullViaUnrealPak(ueQueue, outputDirectory, options);
            foreach (var (pak, entry) in ueQueue)
            {
                var archive = PakArchiveCache.Open(pak, options.PakOpenOptions);
                var rel = PakEntryExtractor.NormalizeExtractPath(entry.Path, archive.MountPoint);
                var full = Path.Combine(outputDirectory, rel);
                if (!File.Exists(full) || !IsUsableTextPayload(File.ReadAllBytes(full), entry.Path))
                    continue;

                if (writtenSet.Add(NormalizeKey(pak, entry.Path)))
                    written.Add(full);
            }
        }

        return new PakDataPullResult
        {
            OutputDirectory = outputDirectory,
            Written = written.Count,
            SkippedEncrypted = ueQueue.Count - ueCount,
            UnrealPakExtracted = ueCount,
            WrittenFiles = written,
        };
    }

    private static int PullViaUnrealPak(
        IReadOnlyList<(string PakPath, PakEntryRecord Entry)> entries,
        string outputDirectory,
        PakDataPullOptions options)
    {
        var byPak = entries.GroupBy(e => e.PakPath, StringComparer.OrdinalIgnoreCase);
        var count = 0;
        foreach (var group in byPak)
        {
            var pakPath = group.Key;
            var filter = BuildUnrealPakFilter(group.Select(g => g.Entry), options);
            var scratch = Path.Combine(outputDirectory, ".ue-scratch", Path.GetFileNameWithoutExtension(pakPath));
            Directory.CreateDirectory(scratch);
            try
            {
                UnrealPakRunner.Extract(pakPath, scratch, filter, options.UnrealPakOptions);
                foreach (var file in Directory.EnumerateFiles(scratch, "*", SearchOption.AllDirectories))
                {
                    var rel = Path.GetRelativePath(scratch, file);
                    var dest = Path.Combine(outputDirectory, rel);
                    Directory.CreateDirectory(Path.GetDirectoryName(dest)!);
                    if (File.Exists(dest) && IsUsableTextPayload(File.ReadAllBytes(dest), dest))
                        continue;

                    File.Copy(file, dest, overwrite: true);
                    if (IsUsableTextPayload(File.ReadAllBytes(dest), dest))
                        count++;
                }
            }
            finally
            {
                try { Directory.Delete(scratch, recursive: true); } catch { /* ignore */ }
            }
        }

        return count;
    }

    private static string BuildUnrealPakFilter(
        IEnumerable<PakEntryRecord> entries,
        PakDataPullOptions options)
    {
        if (!string.IsNullOrWhiteSpace(options.Pattern))
            return options.Pattern!;

        var names = entries
            .Select(e => Path.GetFileName(e.Path))
            .Where(n => !string.IsNullOrEmpty(n))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .Take(8)
            .ToList();

        if (names.Count == 1)
            return "*" + names[0] + "*";

        var ext = options.Extensions.FirstOrDefault();
        return string.IsNullOrWhiteSpace(ext) ? "*" : "*" + ext.TrimStart('.') + "*";
    }

    private static string WriteEntry(string outputDirectory, string entryPath, string mountPoint, byte[] data)
    {
        var relative = PakEntryExtractor.NormalizeExtractPath(entryPath, mountPoint);
        var target = Path.Combine(outputDirectory, relative);
        Directory.CreateDirectory(Path.GetDirectoryName(target)!);
        File.WriteAllBytes(target, data);
        return target;
    }

    private static bool IsUsableTextPayload(byte[] data, string path) =>
        PakPayloadDecoder.LooksLikeTextPayload(data, path);

    private static PakSearchOptions ToSearchOptions(PakDataPullOptions options) =>
        new()
        {
            Pattern = options.Pattern,
            Extensions = options.Extensions,
            MaxResults = options.MaxFiles > 0 ? options.MaxFiles : int.MaxValue,
        };

    private static string NormalizeKey(string pakPath, string entryPath) =>
        pakPath + "|" + entryPath;
}
