using System.Text.Json;
using CsStratware.Core.Json;

namespace CsStratware.Infrastructure.Caching;

public sealed class ExtractionCacheRecord
{
    public string PakPath { get; set; } = "";
    public string PakIdentity { get; set; } = "";
    public string Filter { get; set; } = "";
    public string ExtractDir { get; set; } = "";
    public string ManifestSha256 { get; set; } = "";
    public DateTime ExtractedUtc { get; set; }
}

public sealed class ExtractionCache
{
    private readonly string _storePath;

    public ExtractionCache(string? modRoot = null)
    {
        var root = SharedCacheStore.ExtractionDir(modRoot);
        Directory.CreateDirectory(root);
        _storePath = Path.Combine(root, "records.json");
    }

    public bool TryGetValid(string pakPath, string filter, out string extractDir)
    {
        extractDir = "";
        var records = Load();
        var pakId = File.Exists(pakPath) ? FileIdentity.FromPath(pakPath).CacheKey : "";
        var record = records.FirstOrDefault(r =>
            string.Equals(r.PakPath, Path.GetFullPath(pakPath), StringComparison.OrdinalIgnoreCase)
            && string.Equals(r.Filter, filter, StringComparison.OrdinalIgnoreCase)
            && r.PakIdentity == pakId);

        if (record is null || !Directory.Exists(record.ExtractDir))
            return false;

        var manifest = Path.Combine(record.ExtractDir, ".extraction-manifest.sha256");
        if (!File.Exists(manifest))
            return false;

        var onDisk = File.ReadAllText(manifest).Trim();
        if (!string.Equals(onDisk, record.ManifestSha256, StringComparison.OrdinalIgnoreCase))
            return false;

        extractDir = record.ExtractDir;
        return true;
    }

    public void Register(string pakPath, string filter, string extractDir, IEnumerable<string> extractedFiles)
    {
        var manifestHash = ContentHasher.HashText(string.Join('\n',
            extractedFiles.OrderBy(f => f, StringComparer.OrdinalIgnoreCase)
                .Select(f => $"{f}:{ContentHasher.HashFile(f)}")));

        Directory.CreateDirectory(extractDir);
        File.WriteAllText(Path.Combine(extractDir, ".extraction-manifest.sha256"), manifestHash);

        var records = Load();
        records.RemoveAll(r =>
            string.Equals(r.PakPath, Path.GetFullPath(pakPath), StringComparison.OrdinalIgnoreCase)
            && string.Equals(r.Filter, filter, StringComparison.OrdinalIgnoreCase));

        records.Add(new ExtractionCacheRecord
        {
            PakPath = Path.GetFullPath(pakPath),
            PakIdentity = FileIdentity.FromPath(pakPath).CacheKey,
            Filter = filter,
            ExtractDir = extractDir,
            ManifestSha256 = manifestHash,
            ExtractedUtc = DateTime.UtcNow,
        });

        Save(records);
    }

    private List<ExtractionCacheRecord> Load()
    {
        if (!File.Exists(_storePath))
            return [];

        try
        {
            return JsonSerializer.Deserialize<List<ExtractionCacheRecord>>(
                       File.ReadAllText(_storePath), StratwareJson.Options)
                   ?? [];
        }
        catch
        {
            return [];
        }
    }

    private void Save(List<ExtractionCacheRecord> records)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(_storePath)!);
        File.WriteAllText(_storePath, JsonSerializer.Serialize(records, StratwareJson.Options));
    }
}
