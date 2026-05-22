using UTool.Infrastructure.IO;

namespace UTool.Infrastructure.PlayerData;

public sealed class PlayerDataStore
{
    public string RootPath { get; }

    public PlayerDataStore(string rootPath)
    {
        if (string.IsNullOrWhiteSpace(rootPath))
            throw new ArgumentException("Player data root is required.", nameof(rootPath));

        RootPath = Path.GetFullPath(rootPath);
    }

    public bool Exists => Directory.Exists(RootPath);

    public IReadOnlyList<string> ListProfileIds()
    {
        if (!Exists)
            return [];

        return Directory
            .EnumerateDirectories(RootPath)
            .Select(Path.GetFileName)
            .Where(name => name is not null && PlayerDataFileFilter.LooksLikeProfileId(name))
            .Cast<string>()
            .OrderBy(id => id, StringComparer.Ordinal)
            .ToList();
    }

    public IEnumerable<PlayerDataFileRef> EnumerateJsonFiles(string? profileId = null, bool recursive = true)
    {
        if (!Exists)
            yield break;

        var profiles = profileId is null
            ? ListProfileIds()
            : [profileId];

        foreach (var id in profiles)
        {
            var profileDir = Path.Combine(RootPath, id);
            if (!Directory.Exists(profileDir))
                continue;

            var option = recursive ? SearchOption.AllDirectories : SearchOption.TopDirectoryOnly;
            foreach (var path in Directory.EnumerateFiles(profileDir, "*.json", option))
            {
                var name = Path.GetFileName(path);
                if (!PlayerDataFileFilter.IsWritableSaveFile(name))
                    continue;

                var relative = Path.GetRelativePath(profileDir, path);
                yield return new PlayerDataFileRef(id, relative.Replace('\\', '/'), path);
            }
        }
    }

    public string? ResolveProfileFile(string profileId, string relativePath)
    {
        var full = Path.Combine(RootPath, profileId, relativePath.Replace('/', Path.DirectorySeparatorChar));
        return File.Exists(full) ? full : null;
    }

    public async Task<string> ReadTextAsync(PlayerDataFileRef file, CancellationToken cancellationToken = default) =>
        await StreamingFileOps.ReadTextAsync(file.FullPath, cancellationToken).ConfigureAwait(false);

    public async Task WriteTextAsync(
        PlayerDataFileRef file,
        string content,
        bool createBackup = true,
        CancellationToken cancellationToken = default)
    {
        if (createBackup && File.Exists(file.FullPath))
        {
            var backup = file.FullPath + ".backup";
            await StreamingFileOps.CopyFileAsync(file.FullPath, backup, overwrite: true, cancellationToken)
                .ConfigureAwait(false);
        }

        var dir = Path.GetDirectoryName(file.FullPath)!;
        Directory.CreateDirectory(dir);
        await StreamingFileOps.WriteTextAsync(file.FullPath, content, cancellationToken).ConfigureAwait(false);
    }
}

public readonly record struct PlayerDataFileRef(string ProfileId, string RelativePath, string FullPath);
