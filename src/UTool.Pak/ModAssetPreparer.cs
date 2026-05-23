using UTool.Core.Models;
using UTool.Infrastructure.Build;
using UTool.Infrastructure.PlayerData;
using UTool.Infrastructure.Caching;
using UTool.Infrastructure.IO;
using UTool.Infrastructure.Logging;
using UTool.Infrastructure.Operations;
using UTool.Infrastructure.Pipeline;
using UTool.Infrastructure.Validation;
using UTool.ModLoader;
using UTool.ModLoader.Curves;

namespace UTool.Pak;

public sealed class ModPrepareOptions
{
    public string? SourcePakPath { get; init; }
    public IReadOnlyList<string> CurveSourcePakPaths { get; init; } = [];
    public byte[]? PakAesKey { get; init; }
    public UnrealPakOptions? UnrealPakOptions { get; init; }
    public string? ExtractedDir { get; init; }
    public string? UnrealPakExecutable { get; init; }
    public bool ForceExtract { get; init; }
    public string? CompiledAssemblyPath { get; init; }
    public string? PlayerDataRoot { get; init; }
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

        using var _ = UToolLog.Timed($"prepare {mod.Manifest.Id}");

        var saves = PlayerSaveReader.TryLoad(options.PlayerDataRoot);
        var codePatches = LoadCodePatches(mod, options, saves);
        var jsonPatchesByAsset = LoadJsonPatches(mod);

        var candidatePaths = jsonPatchesByAsset.Keys
            .Concat(codePatches.Select(p => p.AssetFileName))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .ToList();

        var assetPaths = candidatePaths
            .Where(path => AssetShouldPrepare(path, jsonPatchesByAsset, codePatches, saves))
            .ToList();

        var curvePatches = LoadCodeCurvePatches(mod, options);
        var hasCurvePatches = HasJsonCurvePatches(mod) || curvePatches.Count > 0;
        if (assetPaths.Count == 0 && !hasCurvePatches)
        {
            var reason = codePatches.Count > 0 && saves is not null
                ? "conditional asset patch did not apply (check PlayerData / boss completion)"
                : "add [PatchAsset] code patches, patchFiles, or curves/*.curve.json";
            throw new InvalidOperationException($"No assets to prepare for mod '{mod.Manifest.Id}': {reason}.");
        }

        var inputHashes = BuildInputHashes(mod, options, jsonPatchesByAsset);
        var expectedOutputs = assetPaths
            .Select(a => Path.Combine(preparedRoot, Path.GetFileName(a)))
            .ToList();

        var incremental = new IncrementalBuildTracker(mod.RootPath, "prepare");
        if (options.SkipIfUpToDate
            && !options.ForceExtract
            && incremental.IsUpToDate(inputHashes, expectedOutputs))
        {
            UToolLog.Info("prepare skipped (incremental)", new { mod = mod.Manifest.Id });
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

        var preparedFiles = assetPaths.Count == 0
            ? []
            : ParallelPatchPipeline.Map(
                assetPaths,
                assetPath => PrepareOneAsset(
                    mod,
                    assetPath,
                    preparedRoot,
                    options,
                    jsonPatchesByAsset,
                    codePatches,
                    saves,
                    extractionPipeline),
                op).ToList();

        if (hasCurvePatches)
        {
            var curveFiles = ModCurvePreparer.Prepare(
                mod,
                preparedRoot,
                new ModCurvePrepareOptions
                {
                    SourcePakPaths = options.CurveSourcePakPaths,
                    AesKey = options.PakAesKey,
                    UnrealPakOptions = options.UnrealPakOptions,
                    ForceRefresh = options.ForceExtract,
                },
                curvePatches);
            preparedFiles.AddRange(curveFiles);
        }

        incremental.Record("prepare", inputHashes, preparedFiles);

        return new ModPrepareResult
        {
            PreparedContentDir = preparedRoot,
            PreparedFiles = preparedFiles,
        };
    }

    private static bool HasJsonCurvePatches(ModPackage mod)
    {
        var dir = Path.Combine(mod.RootPath, mod.Manifest.CurvePatchesDir ?? "curves");
        return Directory.Exists(dir)
            && Directory.EnumerateFiles(dir, "*.curve.json", SearchOption.TopDirectoryOnly).Any();
    }

    private static IReadOnlyList<CodeCurvePatch> LoadCodeCurvePatches(ModPackage mod, ModPrepareOptions options)
    {
        var assembly = options.CompiledAssemblyPath;
        if (string.IsNullOrWhiteSpace(assembly))
        {
            if (!ModCodeCompiler.HasCodeProject(mod))
                return [];

            assembly = ModCodeCompiler.Compile(mod).AssemblyPath;
        }

        return ModCodePatchRunner.LoadCurvePatches(assembly, mod.Manifest.Id);
    }

    private static string PrepareOneAsset(
        ModPackage mod,
        string assetPath,
        string preparedRoot,
        ModPrepareOptions options,
        Dictionary<string, List<PatchOperation>> jsonPatchesByAsset,
        IReadOnlyList<CodeAssetPatch> codePatches,
        PlayerSaveReader? saves,
        UnrealPakExtractionPipeline extractionPipeline)
    {
        var patchers = ModCodePatchRunner.FilterActivePatches(
            codePatches.Where(p => string.Equals(p.AssetFileName, assetPath, StringComparison.OrdinalIgnoreCase)),
            saves);

        var sourceJson = LoadSourceJson(mod, assetPath, options, extractionPipeline);
        var issues = JsonSchemaValidator.ValidateUeExport(sourceJson);
        foreach (var issue in issues)
            UToolLog.Warn($"schema: {assetPath}", new { issue });

        var current = sourceJson;

        if (jsonPatchesByAsset.TryGetValue(assetPath, out var jsonOps))
            current = JsonAssetPatcher.Apply(current, jsonOps);

        if (patchers.Count > 0)
            current = ModCodePatchRunner.ApplyAll(current, patchers, saves);

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

        var curvesDir = Path.Combine(mod.RootPath, mod.Manifest.CurvePatchesDir ?? "curves");
        if (Directory.Exists(curvesDir))
        {
            foreach (var curve in Directory.EnumerateFiles(curvesDir, "*.curve.json"))
                hashes[curve] = ContentHasher.HashFile(curve);
        }

        if (!string.IsNullOrWhiteSpace(options.CompiledAssemblyPath) && File.Exists(options.CompiledAssemblyPath))
            hashes[options.CompiledAssemblyPath] = ContentHasher.HashFile(options.CompiledAssemblyPath);

        if (!string.IsNullOrWhiteSpace(options.SourcePakPath) && File.Exists(options.SourcePakPath))
            hashes[options.SourcePakPath] = FileIdentity.FromPath(options.SourcePakPath).CacheKey;

        foreach (var kv in jsonPatches)
            hashes[$"ops:{kv.Key}"] = ContentHasher.HashText(string.Join('|', kv.Value.Select(o =>
                $"{o.Op}:{o.Path}:{o.MatchProperty}:{o.MatchValue}:{o.TargetPath}")));

        return hashes;
    }

    private static bool AssetShouldPrepare(
        string assetPath,
        Dictionary<string, List<PatchOperation>> jsonPatchesByAsset,
        IReadOnlyList<CodeAssetPatch> codePatches,
        PlayerSaveReader? saves)
    {
        if (jsonPatchesByAsset.ContainsKey(assetPath))
            return true;

        var forAsset = codePatches
            .Where(p => string.Equals(p.AssetFileName, assetPath, StringComparison.OrdinalIgnoreCase));
        return ModCodePatchRunner.FilterActivePatches(forAsset, saves).Count > 0;
    }

    private static IReadOnlyList<CodeAssetPatch> LoadCodePatches(
        ModPackage mod,
        ModPrepareOptions options,
        PlayerSaveReader? saves)
    {
        var assembly = options.CompiledAssemblyPath;
        if (string.IsNullOrWhiteSpace(assembly))
        {
            if (!ModCodeCompiler.HasCodeProject(mod))
                return [];

            assembly = ModCodeCompiler.Compile(mod).AssemblyPath;
        }

        var bundle = ModCodePatchRunner.LoadFromAssembly(assembly, mod.Manifest.Id);
        if (bundle.AssetPatches.Count == 0 && bundle.PlayerDataPatches.Count > 0)
            UToolLog.Info("prepare: no asset patches (playerdata-only mod)", new { mod = mod.Manifest.Id });

        if (bundle.AssetPatches.Count == 0 && mod.Manifest.PatchFiles.Count == 0)
            return [];

        return bundle.AssetPatches;
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

        if (!string.IsNullOrWhiteSpace(options.ExtractedDir) && Directory.Exists(options.ExtractedDir))
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
