using CsStratware.Infrastructure.IO;
using CsStratware.Infrastructure.PlayerData;
using CsStratware.Sdk;

namespace CsStratware.ModLoader;

public sealed class PlayerSaveReader : IPlayerSaveContext
{
    private readonly PlayerDataStore _store;

    public PlayerSaveReader(PlayerDataStore store) => _store = store;

    public static PlayerSaveReader? TryLoad(string? rootPath)
    {
        if (string.IsNullOrWhiteSpace(rootPath))
            return null;

        var store = new PlayerDataStore(rootPath);
        return store.Exists ? new PlayerSaveReader(store) : null;
    }

    public IReadOnlyList<string> ProfileIds => _store.ListProfileIds();

    public bool AnyProfileHasCompletedAccolade(string rowName, string dataTableName = "D_Accolades")
    {
        foreach (var id in ProfileIds)
        {
            if (ProfileHasCompletedAccolade(id, rowName, dataTableName))
                return true;
        }

        return false;
    }

    public bool ProfileHasCompletedAccolade(string profileId, string rowName, string? dataTableName = "D_Accolades")
    {
        var path = _store.ResolveProfileFile(profileId, "Accolades.json");
        if (path is null)
            return false;

        var json = StreamingFileOps.ReadTextAsync(path).GetAwaiter().GetResult();
        return AccoladeQuery.HasCompletedAccolade(json, rowName, dataTableName);
    }

    public string? FindProfileFile(string relativePath)
    {
        foreach (var id in ProfileIds)
        {
            var hit = _store.ResolveProfileFile(id, relativePath);
            if (hit is not null)
                return hit;
        }

        return null;
    }

    public bool ProfileFileExists(string profileId, string relativePath) =>
        _store.ResolveProfileFile(profileId, relativePath) is not null;
}
