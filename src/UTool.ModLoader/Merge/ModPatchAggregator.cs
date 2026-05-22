using UTool.Core.Models;

namespace UTool.ModLoader.Merge;

/// <summary>Collect declarative patch operations from mods in load order.</summary>
public static class ModPatchAggregator
{
    public static IReadOnlyList<JsonModificationSet> CollectPatchOperations(IReadOnlyList<ModPackage> modsInLoadOrder)
    {
        var sets = new List<JsonModificationSet>();
        foreach (var mod in modsInLoadOrder)
        {
            foreach (var patchFile in mod.Manifest.PatchFiles)
            {
                var patchPath = Path.Combine(mod.RootPath, patchFile);
                if (!File.Exists(patchPath))
                    continue;

                var doc = AssetPatchReader.Read(patchPath);
                foreach (var patch in doc.Patches)
                {
                    sets.Add(new JsonModificationSet
                    {
                        SourceModId = mod.Manifest.Id,
                        AssetPath = patch.AssetPath,
                        Operations = patch.Operations
                            .Select(op => JsonPatchOperation.FromPatchOperation(mod.Manifest.Id, patch.AssetPath, op))
                            .ToList(),
                    });
                }
            }
        }

        return sets;
    }

    public static Dictionary<string, List<JsonPatchOperation>> GroupByAsset(
        IReadOnlyList<JsonModificationSet> sets)
    {
        var map = new Dictionary<string, List<JsonPatchOperation>>(StringComparer.OrdinalIgnoreCase);
        foreach (var set in sets)
        {
            if (!map.TryGetValue(set.AssetPath, out var ops))
            {
                ops = [];
                map[set.AssetPath] = ops;
            }

            ops.AddRange(set.Operations);
        }

        return map;
    }
}
