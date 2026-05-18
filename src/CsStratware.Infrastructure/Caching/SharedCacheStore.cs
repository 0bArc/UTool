namespace CsStratware.Infrastructure.Caching;

public static class SharedCacheStore
{
    public static string ResolveRoot(string? modRoot = null)
    {
        if (!string.IsNullOrWhiteSpace(modRoot))
            return Path.Combine(modRoot, ".cache", "shared");

        var local = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        return Path.Combine(local, "csmanager", "cache");
    }

    public static string PakIndexDir(string? modRoot = null) =>
        Path.Combine(ResolveRoot(modRoot), "pak-index");

    public static string ExtractionDir(string? modRoot = null) =>
        Path.Combine(ResolveRoot(modRoot), "extractions");

    public static string AssetIndexPath(string? modRoot = null) =>
        Path.Combine(ResolveRoot(modRoot), "asset-index.json");
}
