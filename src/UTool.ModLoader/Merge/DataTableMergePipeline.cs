using UTool.Core.Models;

namespace UTool.ModLoader.Merge;

public sealed class ModJsonLayer
{
    public required string SourceId { get; init; }
    public required string Json { get; init; }
}

public sealed class DataTableMergePipelineResult
{
    public required string MergedJson { get; init; }
    public required MergeConflictReport Report { get; init; }
    public required IReadOnlyList<string> AppliedSources { get; init; }
}

/// <summary>Merge base-game JSON with ordered mod layers (load order).</summary>
public static class DataTableMergePipeline
{
    public static DataTableMergePipelineResult MergeAsset(
        string baseGameJson,
        IReadOnlyList<ModJsonLayer> modLayers,
        string assetFileName,
        UeDataTableMergeOptions? options = null)
    {
        options ??= new UeDataTableMergeOptions { AssetLabel = assetFileName };
        if (string.IsNullOrWhiteSpace(options.AssetLabel))
            options = new UeDataTableMergeOptions { AssetLabel = assetFileName };

        var chain = new List<string> { baseGameJson };
        chain.AddRange(modLayers.Select(l => l.Json));
        var result = UeDataTableMerger.MergeChain(chain, options);
        return new DataTableMergePipelineResult
        {
            MergedJson = result.Json,
            Report = result.Report,
            AppliedSources = ["base", ..modLayers.Select(l => l.SourceId)],
        };
    }

    public static DataTableMergePipelineResult MergeAssetFromFiles(
        string baseGameJsonPath,
        IReadOnlyList<(string SourceId, string JsonPath)> modLayers,
        string assetFileName,
        string? outputPath = null,
        UeDataTableMergeOptions? options = null)
    {
        var baseJson = File.ReadAllText(baseGameJsonPath);
        var layers = modLayers
            .Select(l => new ModJsonLayer { SourceId = l.SourceId, Json = File.ReadAllText(l.JsonPath) })
            .ToList();

        var merged = MergeAsset(baseJson, layers, assetFileName, options);

        if (outputPath is not null)
            SafeJsonFileWriter.Write(outputPath, merged.MergedJson);

        return merged;
    }

    public static DataTableMergePipelineResult MergeFromPreparedMods(
        string baseGameJson,
        IReadOnlyList<ModPackage> modsInLoadOrder,
        string assetFileName)
    {
        var layers = new List<ModJsonLayer>();
        foreach (var mod in modsInLoadOrder)
        {
            var prepared = Path.Combine(mod.RootPath, ".cache", "prepared", Path.GetFileName(assetFileName));
            if (!File.Exists(prepared))
                continue;

            layers.Add(new ModJsonLayer
            {
                SourceId = mod.Manifest.Id,
                Json = File.ReadAllText(prepared),
            });
        }

        return MergeAsset(baseGameJson, layers, assetFileName);
    }
}
