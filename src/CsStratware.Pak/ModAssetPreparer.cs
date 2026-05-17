using CsStratware.Core.Models;
using CsStratware.ModLoader;

namespace CsStratware.Pak;

public sealed class ModPrepareOptions
{
    public string? SourcePakPath { get; init; }
    public string? ExtractedDir { get; init; }
    public string? UnrealPakExecutable { get; init; }
    public bool ForceExtract { get; init; }
    public string? CompiledAssemblyPath { get; init; }
}

public sealed class ModPrepareResult
{
    public required string PreparedContentDir { get; init; }
    public IReadOnlyList<string> PreparedFiles { get; init; } = [];
}

public static class ModAssetPreparer
{
    public static ModPrepareResult Prepare(
        ModPackage mod,
        ModPrepareOptions? options = null)
    {
        options ??= new ModPrepareOptions();
        var preparedRoot = Path.Combine(mod.RootPath, ".cache", "prepared");
        Directory.CreateDirectory(preparedRoot);

        var codePatches = LoadCodePatches(mod, options);
        var jsonPatchesByAsset = LoadJsonPatches(mod);

        var assetPaths = jsonPatchesByAsset.Keys
            .Concat(codePatches.Select(p => p.AssetFileName))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .ToList();

        if (assetPaths.Count == 0)
            throw new InvalidOperationException($"No assets to prepare for mod '{mod.Manifest.Id}'.");

        var preparedFiles = new List<string>();

        foreach (var assetPath in assetPaths)
        {
            var sourceJson = LoadSourceJson(mod, assetPath, options);
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
            File.WriteAllText(target, current);
            preparedFiles.Add(target);
        }

        return new ModPrepareResult
        {
            PreparedContentDir = preparedRoot,
            PreparedFiles = preparedFiles,
        };
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

        return ModCodePatchRunner.LoadFromAssembly(assembly);
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

    private static string LoadSourceJson(ModPackage mod, string assetPath, ModPrepareOptions options)
    {
        var fileName = Path.GetFileName(assetPath);
        var cacheDir = Path.Combine(mod.RootPath, ".cache", "source");
        Directory.CreateDirectory(cacheDir);
        var cached = Path.Combine(cacheDir, fileName);

        if (!options.ForceExtract && File.Exists(cached) && new FileInfo(cached).Length > 10_000)
            return File.ReadAllText(cached);

        var extractedPath = FindExtractedFile(options.ExtractedDir, fileName);
        if (extractedPath is not null)
        {
            File.Copy(extractedPath, cached, overwrite: true);
            return File.ReadAllText(cached);
        }

        var pak = options.SourcePakPath;
        if (string.IsNullOrWhiteSpace(pak) || !File.Exists(pak))
            throw new FileNotFoundException(
                $"Source JSON not found for '{assetPath}'. Run: pak ue extract <data.pak> ./extracted -filter *{fileName}*");

        var extractDir = Path.Combine(mod.RootPath, ".cache", "ue-extract");
        if (options.ForceExtract && Directory.Exists(extractDir))
        {
            try { Directory.Delete(extractDir, recursive: true); } catch { /* ignore */ }
        }

        Directory.CreateDirectory(extractDir);
        var ue = new UnrealPakOptions { ExecutablePath = options.UnrealPakExecutable };
        UnrealPakRunner.Extract(pak, extractDir, $"*{fileName}*", ue);

        extractedPath = FindExtractedFile(extractDir, fileName)
            ?? throw new FileNotFoundException($"UnrealPak did not extract '{fileName}' from {pak}");

        File.Copy(extractedPath, cached, overwrite: true);
        return File.ReadAllText(cached);
    }

    private static string? FindExtractedFile(string? extractedDir, string fileName)
    {
        if (string.IsNullOrWhiteSpace(extractedDir) || !Directory.Exists(extractedDir))
            return null;

        return Directory
            .EnumerateFiles(extractedDir, fileName, SearchOption.AllDirectories)
            .FirstOrDefault();
    }
}
