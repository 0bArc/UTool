using UTool.Core.Models;
using UTool.Infrastructure.IO;
using UTool.Infrastructure.Logging;
using UTool.ModLoader.Curves;
using UTool.Pak.Models;

namespace UTool.Pak;

public sealed class ModCurvePrepareOptions
{
    public IReadOnlyList<string> SourcePakPaths { get; init; } = [];
    public byte[]? AesKey { get; init; }
    public UnrealPakOptions? UnrealPakOptions { get; init; }
    public bool PreserveSourcePaths { get; init; }
    public bool ForceRefresh { get; init; }
}

public static class ModCurvePreparer
{
    public static IReadOnlyList<string> Prepare(
        ModPackage mod,
        string preparedRoot,
        ModCurvePrepareOptions options,
        IReadOnlyList<CodeCurvePatch>? codePatches = null)
    {
        var curvesDir = Path.Combine(mod.RootPath, mod.Manifest.CurvePatchesDir ?? "curves");
        var specs = CurveFloatPatchReader.ReadDirectory(curvesDir).ToList();
        if (codePatches is { Count: > 0 })
            specs.AddRange(BuildSpecsFromCode(cacheDir: Path.Combine(mod.RootPath, ".cache", "curve-source"), codePatches, options));

        if (specs.Count == 0)
            return [];

        if (options.SourcePakPaths.Count == 0)
        {
            throw new InvalidOperationException(
                $"Mod '{mod.Manifest.Id}' has curve patches but no curve source pak. " +
                "Set pak.curveSourcePak in mod.json (e.g. @paks) or pakAesKey if entries are encrypted.");
        }

        var cacheDir = Path.Combine(mod.RootPath, ".cache", "curve-source");
        var prepared = new List<string>();

        foreach (var spec in specs)
        {
            var assetName = spec.AssetName.EndsWith(".uasset", StringComparison.OrdinalIgnoreCase)
                ? spec.AssetName
                : spec.AssetName + ".uasset";

            var sourcePair = EnsureSourcePair(
                cacheDir,
                options,
                assetName,
                spec.RelativeDirectory);

            var outputRoot = options.PreserveSourcePaths
                ? Path.Combine(preparedRoot, spec.RelativeDirectory.Replace('/', Path.DirectorySeparatorChar))
                : preparedRoot;
            Directory.CreateDirectory(outputRoot);

            var outUasset = Path.Combine(outputRoot, assetName);
            var outUexp = Path.ChangeExtension(outUasset, ".uexp");

            StreamingFileOps.CopyFileAsync(sourcePair.UassetPath, outUasset, overwrite: true).GetAwaiter().GetResult();
            if (File.Exists(sourcePair.UexpPath))
                StreamingFileOps.CopyFileAsync(sourcePair.UexpPath, outUexp, overwrite: true).GetAwaiter().GetResult();

            CurveFloatPatcher.ApplyKeys(outUasset, spec);
            prepared.Add(outUasset);
            if (File.Exists(outUexp))
                prepared.Add(outUexp);

            UToolLog.Info("curve prepared", new { spec.AssetName, outUasset });
        }

        return prepared;
    }

    private sealed record SourcePair(string UassetPath, string UexpPath);

    private static SourcePair EnsureSourcePair(
        string cacheDir,
        ModCurvePrepareOptions options,
        string assetFileName,
        string relativeDirectory)
    {
        var cachedUasset = Path.Combine(cacheDir, relativeDirectory, assetFileName);
        var cachedUexp = Path.ChangeExtension(cachedUasset, ".uexp");
        if (!options.ForceRefresh && File.Exists(cachedUasset))
            return new SourcePair(cachedUasset, cachedUexp);

        Directory.CreateDirectory(Path.GetDirectoryName(cachedUasset)!);
        var pullDir = Path.Combine(cacheDir, ".pull", assetFileName);
        if (Directory.Exists(pullDir))
        {
            try { Directory.Delete(pullDir, recursive: true); } catch { /* ignore */ }
        }

        var pattern = "*" + Path.GetFileNameWithoutExtension(assetFileName) + "*";
        var pull = PakDataPuller.Pull(
            options.SourcePakPaths,
            pullDir,
            new PakDataPullOptions
            {
                Pattern = pattern,
                Extensions = [".uasset", ".uexp"],
                MaxFiles = 4,
                PakOpenOptions = options.AesKey is null ? null : new PakOpenOptions { AesKey = options.AesKey },
                UnrealPakOptions = options.UnrealPakOptions,
            });

        var uasset = pull.WrittenFiles.FirstOrDefault(f =>
            f.EndsWith(".uasset", StringComparison.OrdinalIgnoreCase)
            && f.Contains(Path.GetFileNameWithoutExtension(assetFileName), StringComparison.OrdinalIgnoreCase));

        if (uasset is null)
        {
            throw new FileNotFoundException(
                $"Could not extract '{assetFileName}' from source paks. Pattern: {pattern}");
        }

        var rel = Path.GetRelativePath(pullDir, uasset);
        var targetUasset = Path.Combine(cacheDir, rel);
        Directory.CreateDirectory(Path.GetDirectoryName(targetUasset)!);
        File.Copy(uasset, targetUasset, overwrite: true);

        var uexp = Path.ChangeExtension(uasset, ".uexp");
        var targetUexp = Path.ChangeExtension(targetUasset, ".uexp");
        if (File.Exists(uexp))
            File.Copy(uexp, targetUexp, overwrite: true);

        return new SourcePair(targetUasset, targetUexp);
    }

    private static IEnumerable<CurveFloatPatchSpec> BuildSpecsFromCode(
        string cacheDir,
        IReadOnlyList<CodeCurvePatch> codePatches,
        ModCurvePrepareOptions options)
    {
        foreach (var patch in codePatches)
        {
            var assetName = patch.AssetName.EndsWith(".uasset", StringComparison.OrdinalIgnoreCase)
                ? patch.AssetName
                : patch.AssetName + ".uasset";

            var sourcePair = EnsureSourcePair(
                cacheDir,
                options,
                assetName,
                patch.RelativeDirectory);

            yield return CurveCodePatchRunner.BuildSpecFromUasset(sourcePair.UassetPath, patch);
        }
    }
}
