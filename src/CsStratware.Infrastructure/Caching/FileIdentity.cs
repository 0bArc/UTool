namespace CsStratware.Infrastructure.Caching;

public readonly record struct FileIdentity(string FullPath, long Length, DateTime LastWriteUtc)
{
    public static FileIdentity FromPath(string path)
    {
        var info = new FileInfo(path);
        return new FileIdentity(
            System.IO.Path.GetFullPath(path),
            info.Length,
            info.LastWriteTimeUtc);
    }

    public string CacheKey => $"{Length:x}:{LastWriteUtc.Ticks:x}";
}
