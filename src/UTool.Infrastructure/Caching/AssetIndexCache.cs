using System.Text.Json;
using UTool.Core.Json;

namespace UTool.Infrastructure.Caching;

public sealed class AssetIndexEntry
{
    public required string RelativePath { get; init; }
    public required string FileName { get; init; }
    public required string Sha256 { get; init; }
    public long Size { get; init; }
}

public sealed class AssetIndexManifest
{
    public string RootDirectory { get; set; } = "";
    public string RootSha256 { get; set; } = "";
    public List<AssetIndexEntry> Entries { get; set; } = [];
}

public sealed class AssetIndexCache
{
    private readonly string _rootDirectory;
    private readonly string _manifestPath;
    private AssetIndexManifest? _manifest;

    private AssetIndexCache(string rootDirectory, string manifestPath)
    {
        _rootDirectory = Path.GetFullPath(rootDirectory);
        _manifestPath = manifestPath;
    }

    public static AssetIndexCache ForDirectory(string directory) =>
        new(directory, Path.Combine(directory, ".cache", "asset-index.json"));

    public AssetIndexManifest GetOrBuild(bool forceRebuild = false, CancellationToken cancellationToken = default)
    {
        if (!Directory.Exists(_rootDirectory))
            return new AssetIndexManifest { RootDirectory = _rootDirectory };

        if (!forceRebuild && TryLoad(out var loaded) && loaded is not null)
            return loaded;

        var manifest = Build(cancellationToken);
        Save(manifest);
        _manifest = manifest;
        return manifest;
    }

    public string? FindByFileName(string fileName)
    {
        var manifest = _manifest ?? (TryLoad(out var m) ? m : null);
        if (manifest is null)
            return null;

        var entry = manifest.Entries.FirstOrDefault(e =>
            string.Equals(e.FileName, fileName, StringComparison.OrdinalIgnoreCase));
        return entry is null ? null : Path.Combine(manifest.RootDirectory, entry.RelativePath);
    }

    private bool TryLoad(out AssetIndexManifest? manifest)
    {
        manifest = _manifest;
        if (manifest is not null)
            return true;

        if (!File.Exists(_manifestPath))
            return false;

        try
        {
            var json = File.ReadAllText(_manifestPath);
            manifest = JsonSerializer.Deserialize<AssetIndexManifest>(json, UToolJson.Options);
            if (manifest is null)
                return false;

            var rootId = FileIdentity.FromPath(_rootDirectory);
            if (manifest.RootSha256 != rootId.CacheKey)
            {
                manifest = null;
                return false;
            }

            _manifest = manifest;
            return true;
        }
        catch
        {
            manifest = null;
            return false;
        }
    }

    private AssetIndexManifest Build(CancellationToken cancellationToken)
    {
        var rootId = FileIdentity.FromPath(_rootDirectory);
        var entries = new List<AssetIndexEntry>();

        foreach (var file in Directory.EnumerateFiles(_rootDirectory, "*", SearchOption.AllDirectories))
        {
            cancellationToken.ThrowIfCancellationRequested();
            if (file.Contains($"{Path.DirectorySeparatorChar}.cache{Path.DirectorySeparatorChar}", StringComparison.OrdinalIgnoreCase))
                continue;

            var relative = Path.GetRelativePath(_rootDirectory, file);
            entries.Add(new AssetIndexEntry
            {
                RelativePath = relative,
                FileName = Path.GetFileName(file),
                Sha256 = ContentHasher.HashFile(file),
                Size = new FileInfo(file).Length,
            });
        }

        return new AssetIndexManifest
        {
            RootDirectory = _rootDirectory,
            RootSha256 = rootId.CacheKey,
            Entries = entries,
        };
    }

    private void Save(AssetIndexManifest manifest)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(_manifestPath)!);
        var json = JsonSerializer.Serialize(manifest, UToolJson.Options);
        File.WriteAllText(_manifestPath, json);
        _manifest = manifest;
    }
}
