namespace CsStratware.Infrastructure.Caching;

public readonly record struct FileIdentity(string FullPath, long Length, DateTime LastWriteUtc)
{
    public static FileIdentity FromPath(string path)
    {
        var fullPath = Path.GetFullPath(path);
        if (Directory.Exists(fullPath))
        {
            var dir = new DirectoryInfo(fullPath);
            return new FileIdentity(fullPath, -1, dir.LastWriteTimeUtc);
        }

        if (!File.Exists(fullPath))
            return new FileIdentity(fullPath, 0, DateTime.MinValue);

        var info = new FileInfo(fullPath);
        return new FileIdentity(fullPath, info.Length, info.LastWriteTimeUtc);
    }

    public string CacheKey => $"{Length:x}:{LastWriteUtc.Ticks:x}";
}
