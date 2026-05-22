using UTool.Infrastructure.PlayerData;
using UTool.Sdk;

namespace UTool.ModLoader;

public sealed class PlayerDataApplyResult
{
    public required string ProfileId { get; init; }
    public required string RelativePath { get; init; }
    public required string FullPath { get; init; }
    public bool Changed { get; init; }
}

public static class ModPlayerDataPatchRunner
{
    public static IReadOnlyList<PlayerDataApplyResult> ApplyAll(
        PlayerDataStore store,
        IEnumerable<CodePlayerDataPatch> patches,
        string? profileId = null,
        bool dryRun = false,
        CancellationToken cancellationToken = default)
    {
        var saves = new PlayerSaveReader(store);
        var results = new List<PlayerDataApplyResult>();
        var profiles = profileId is null ? store.ListProfileIds() : [profileId];

        foreach (var id in profiles)
        {
            foreach (var patch in patches)
            {
                var path = store.ResolveProfileFile(id, patch.RelativePath);
                if (path is null)
                    continue;

                var before = store.ReadTextAsync(new PlayerDataFileRef(id, patch.RelativePath, path), cancellationToken)
                    .GetAwaiter()
                    .GetResult();
                var after = ModCodePatchRunner.ApplyPlayerData(before, patch.Instance, new PlayerDataApplyContext
                {
                    ProfileId = id,
                    RelativePath = patch.RelativePath,
                    FullPath = path,
                    Saves = saves,
                });

                var changed = !string.Equals(before, after, StringComparison.Ordinal);
                if (changed && !dryRun)
                {
                    store.WriteTextAsync(
                            new PlayerDataFileRef(id, patch.RelativePath, path),
                            after,
                            cancellationToken: cancellationToken)
                        .GetAwaiter()
                        .GetResult();
                }

                results.Add(new PlayerDataApplyResult
                {
                    ProfileId = id,
                    RelativePath = patch.RelativePath,
                    FullPath = path,
                    Changed = changed,
                });
            }
        }

        return results;
    }
}
