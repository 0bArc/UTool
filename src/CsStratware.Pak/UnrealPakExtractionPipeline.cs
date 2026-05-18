using CsStratware.Infrastructure.Caching;
using CsStratware.Infrastructure.Logging;

namespace CsStratware.Pak;

public sealed class UnrealPakExtractionPipeline
{
    private readonly ExtractionCache _cache;
    private readonly UnrealPakOptions? _ueOptions;

    public UnrealPakExtractionPipeline(string? modRoot = null, UnrealPakOptions? ueOptions = null)
    {
        _cache = new ExtractionCache(modRoot);
        _ueOptions = ueOptions;
    }

    public string ExtractFiltered(
        string pakPath,
        string filter,
        string? preferredDir = null,
        bool force = false)
    {
        pakPath = Path.GetFullPath(pakPath);
        filter ??= "*";

        if (!force && _cache.TryGetValid(pakPath, filter, out var cached))
        {
            StratwareLog.Debug("extraction cache hit", new { pakPath, filter, cached });
            return cached;
        }

        var extractDir = preferredDir
            ?? Path.Combine(
                SharedCacheStore.ExtractionDir(),
                ContentHasher.HashText($"{pakPath}|{filter}")[..16]);

        if (force && Directory.Exists(extractDir))
        {
            try { Directory.Delete(extractDir, recursive: true); } catch { /* ignore */ }
        }

        Directory.CreateDirectory(extractDir);
        UnrealPakRunner.Extract(pakPath, extractDir, filter, _ueOptions);

        var files = Directory.EnumerateFiles(extractDir, "*", SearchOption.AllDirectories).ToList();
        _cache.Register(pakPath, filter, extractDir, files);
        return extractDir;
    }
}
