using CsStratware.Core.Models;
using CsStratware.Infrastructure.Build;
using CsStratware.Infrastructure.Caching;
using CsStratware.Infrastructure.IO;
using CsStratware.Infrastructure.Logging;
using CsStratware.Infrastructure.Operations;
using CsStratware.Infrastructure.Pipeline;
using CsStratware.Infrastructure.Validation;
using CsStratware.ModLoader;

namespace CsStratware.Pak;

public sealed class ModPrepareOptions
{
    public string? SourcePakPath { get; init; }
    public string? ExtractedDir { get; init; }
    public string? UnrealPakExecutable { get; init; }
    public bool ForceExtract { get; init; }
    public string? CompiledAssemblyPath { get; init; }
    public OperationContext? Operation { get; init; }
    public bool SkipIfUpToDate { get; init; } = true;
}

public sealed class ModPrepareResult
{
    public required string PreparedContentDir { get; init; }
    public IReadOnlyList<string> PreparedFiles { get; init; } = [];
    public bool FromIncrementalCache { get; init; }
}

public static class ModAssetPreparer
{
    public static ModPrepareResult Prepare(
        ModPackage mod,
        ModPrepareOptions? options = null)
    {
        options ??= new ModPrepareOptions();
        var op = options.Operation ?? new OperationContext();
        var preparedRoot = Path.Combine(mod.RootPath, ".cache", "prepared");
        Directory.CreateDirectory(preparedRoot);

        using var _ = StratwareLog.Timed($"prepare {mod.Manifest.Id}");

        var codePatches = LoadCodePatches(mod, options);
        var jsonPatchesByAsset = LoadJsonPatches(mod);

        var assetPaths = jsonPatchesByAsset.Keys
            .Concat(codePatches.Select(p => p.AssetFileName))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .ToList();

        if (assetPaths.Count == 0)
            throw new InvalidOperationException($"No assets to prepare for mod '{mod.Manifest.Id}'.");

        var inputHashes = BuildInputHashes(mod, options, jsonPatchesByAsset);
        var expectedOutputs = assetPaths
            .Select(a => Path.Combine(preparedRoot, Path.GetFileName(a)))
            .ToList();

        var incremental = new IncrementalBuildTracker(mod.RootPath, "prepare");
        if (options.SkipIfUpToDate
            && !options.ForceExtract
            && incremental.IsUpToDate(inputHashes, expectedOutputs))
        {
            StratwareLog.Info("prepare skipped (incremental)", new { mod = mod.Manifest.Id });
            return new ModPrepareResult
            {
                PreparedContentDir = preparedRoot,
                PreparedFiles = expectedOutputs.Where(File.Exists).ToList(),
                FromIncrementalCache = true,
            };
        }

        var extractionPipeline = new UnrealPakExtractionPipeline(mod.RootPath, new UnrealPakOptions
        {
            ExecutablePath = options.UnrealPakExecutable,
        });

        var preparedFiles = ParallelPatchPipeline.Map(
            assetPaths,
            assetPath => PrepareOneAsset(
                mod,
                assetPath,
                preparedRoot,
                options,
                jsonPatchesByAsset,
                codePatches,
                extractionPipeline),
            op);

        incremental.Record("prepare", inputHashes, preparedFiles.ToList());

        return new ModPrepareResult
        {
            PreparedContentDir = preparedRoot,
            PreparedFiles = preparedFiles,
        };
    }

    private static string PrepareOneAsset(
        ModPackage mod,
        string assetPath,
        string preparedRoot,
        ModPrepareOptions options,
        Dictionary<string, List<PatchOperation>> jsonPatchesByAsset,
        IReadOnlyList<CodeAssetPatch> codePatches,
        UnrealPakExtractionPipeline extractionPipeline)
    {
        var sourceJson = LoadSourceJson(mod, assetPath, options, extractionPipeline);
        var issues = JsonSchemaValidator.ValidateUeExport(sourceJson);
        foreach (var issue in issues)
            StratwareLog.Warn($"schema: {assetPath}", new { issue });

        var current = sourceJson;

        if (jsonPatchesByAsset.TryGetValue(assetPath, out var jsonOps))
            current = JsonAssetPatcher.Apply(current, jsonOps);

        var patchers = codePatches
            .Where(p => string.Equals(p.AssetFileName, assetPath, StringComparison.OrdinalIgnoreCase))
            .Select(p => p.Instance)
            .ToList();

        if (patchers.Count > 0)
            current = ModCodePatchRunner.ApplyAll(current, patchers);

        var stageName = Path.GetFileName(assetPath);
        var target = Path.Combine(preparedRoot, stageName);
        Directory.CreateDirectory(Path.GetDirectoryName(target)!);
        StreamingFileOps.WriteTextAsync(target, current).GetAwaiter().GetResult();
        return target;
    }

    private static Dictionary<string, string> BuildInputHashes(
        ModPackage mod,
        ModPrepareOptions options,
        Dictionary<string, List<PatchOperation>> jsonPatches)
    {
        var hashes = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        hashes[Path.Combine(mod.RootPath, ModManifestReader.ManifestFileName)] =
            ContentHasher.HashFile(Path.Combine(mod.RootPath, ModManifestReader.ManifestFileName));

        foreach (var patchFile in mod.Manifest.PatchFiles)
        {
            var path = Path.Combine(mod.RootPath, patchFile);
            if (File.Exists(path))
                hashes[path] = ContentHasher.HashFile(path);
        }

        if (!string.IsNullOrWhiteSpace(options.CompiledAssemblyPath) && File.Exists(options.CompiledAssemblyPath))
            hashes[options.CompiledAssemblyPath] = ContentHasher.HashFile(options.CompiledAssemblyPath);

        if (!string.IsNullOrWhiteSpace(options.SourcePakPath) && File.Exists(options.SourcePakPath))
            hashes[options.SourcePakPath] = FileIdentity.FromPath(options.SourcePakPath).CacheKey;

        foreach (var kv in jsonPatches)
            hashes[$"ops:{kv.Key}"] = ContentHasher.HashText(string.Join('|', kv.Value.Select(o => $"{o.Op}:{o.Path}")));

        return hashes;
    }

    private static IReadOnlyList<CodeAssetPatch> LoadCodePatches(ModPackage mod, ModPrepareOptions options)
    {
        var assembly = options.CompiledAssemblyPath;
        if (string.IsNullOrWhiteSpace(assembly))
        {
            if (!ModCodeCompiler.HasCodeProject(mod))
                return [];

            assembly = ModCodeCompiler.Compile(mod).AssemblyPath;
        }

        return ModCodePatchRunner.LoadFromAssembly(assembly, mod.Manifest.Id);
    }

    private static Dictionary<string, List<PatchOperation>> LoadJsonPatches(ModPackage mod)
    {
        var byAsset = new Dictionary<string, List<PatchOperation>>(StringComparer.OrdinalIgnoreCase);

        foreach (var patchFile in mod.Manifest.PatchFiles)
        {
            var patchPath = Path.Combine(mod.RootPath, patchFile);
            if (!File.Exists(patchPath))
                throw new FileNotFoundException($"Patch file missing: {patchPath}");

            var doc = AssetPatchReader.Read(patchPath);
            foreach (var patch in doc.Patches)
            {
                if (!byAsset.TryGetValue(patch.AssetPath, out var ops))
                {
                    ops = [];
                    byAsset[patch.AssetPath] = ops;
                }

                ops.AddRange(patch.Operations);
            }
        }

        return byAsset;
    }

    private static string LoadSourceJson(
        ModPackage mod,
        string assetPath,
        ModPrepareOptions options,
        UnrealPakExtractionPipeline extractionPipeline)
    {
        var fileName = Path.GetFileName(assetPath);
        var cacheDir = Path.Combine(mod.RootPath, ".cache", "source");
        Directory.CreateDirectory(cacheDir);
        var cached = Path.Combine(cacheDir, fileName);
        var cacheMeta = cached + ".sha256";

        if (!options.ForceExtract && File.Exists(cached) && File.Exists(cacheMeta))
        {
            var expected = File.ReadAllText(cacheMeta).Trim();
            if (string.Equals(ContentHasher.HashFile(cached), expected, StringComparison.OrdinalIgnoreCase))
                return StreamingFileOps.ReadTextAsync(cached).GetAwaiter().GetResult();
        }

        if (!string.IsNullOrWhiteSpace(options.ExtractedDir))
        {
            var index = AssetIndexCache.ForDirectory(options.ExtractedDir);
            index.GetOrBuild(cancellationToken: options.Operation?.CancellationToken ?? default);
            var indexed = index.FindByFileName(fileName);
            if (indexed is not null)
            {
                CacheSource(cached, cacheMeta, indexed);
                return StreamingFileOps.ReadTextAsync(cached).GetAwaiter().GetResult();
            }
        }

        var extractedPath = FindExtractedFile(options.ExtractedDir, fileName);
        if (extractedPath is not null)
        {
            CacheSource(cached, cacheMeta, extractedPath);
            return StreamingFileOps.ReadTextAsync(cached).GetAwaiter().GetResult();
        }

        var pak = options.SourcePakPath;
        if (string.IsNullOrWhiteSpace(pak) || !File.Exists(pak))
        {
            throw new FileNotFoundException(
                $"Source JSON not found for '{assetPath}'. Run: pak ue extract <data.pak> ./extracted -filter *{fileName}*");
        }

        var filter = $"*{fileName}*";
        var extractDir = extractionPipeline.ExtractFiltered(
            pak,
            filter,
            Path.Combine(mod.RootPath, ".cache", "ue-extract", ContentHasher.HashText(filter)[..12]),
            options.ForceExtract);

        extractedPath = FindExtractedFile(extractDir, fileName)
            ?? throw new FileNotFoundException($"UnrealPak did not extract '{fileName}' from {pak}");

        CacheSource(cached, cacheMeta, extractedPath);
        return StreamingFileOps.ReadTextAsync(cached).GetAwaiter().GetResult();
    }

    private static void CacheSource(string cached, string cacheMeta, string extractedPath)
    {
        StreamingFileOps.CopyFileAsync(extractedPath, cached, overwrite: true).GetAwaiter().GetResult();
        File.WriteAllText(cacheMeta, ContentHasher.HashFile(cached));
    }

    private static string? FindExtractedFile(string? extractedDir, string fileName)
    {
        if (string.IsNullOrWhiteSpace(extractedDir) || !Directory.Exists(extractedDir))
            return null;

        var index = AssetIndexCache.ForDirectory(extractedDir);
        var hit = index.FindByFileName(fileName);
        if (hit is not null)
            return hit;

        return Directory
            .EnumerateFiles(extractedDir, fileName, SearchOption.AllDirectories)
            .FirstOrDefault();
    }
}
