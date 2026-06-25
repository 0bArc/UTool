using UTool.Core.Models;
using UTool.ModLoader;
using UTool.ModLoader.Merge;
using UTool.Pak;

namespace UTool.Cli;

internal static partial class PakCommands
{
    private static int Merge(string[] args)
    {
        var output = CliArgs.GetArgAny(args, "-o", "--output");
        if (output is null)
            return Missing("merge <pak1> <pak2> [...] -o <out.pak>");

        var cfg = UToolConfig.Load();
        var gameId = CliArgs.GetArg(args, "--game");
        var paks = CollectPositionalPakArgs(args, output);
        if (paks.Count == 0)
            return Missing("merge <pak1> <pak2> [...] -o <out.pak>");

        var mount = CliArgs.GetArg(args, "--mount");
        var buildOptions = mount is null ? null : new PakBuildOptions { MountPoint = mount };
        var pakOptions = ResolvePakOpenOptions(args, cfg, gameId);
        var ue = UnrealPakToolchain.ToOptions(cfg.ResolveUnrealPakToolchain(), pakOptions?.AesKey);
        var extractDir = CliArgs.GetArg(args, "--extract-dir");
        var verbose = CliArgs.IsVerbose(args);
        var result = PakMerger.Merge(paks, output, new PakMergeOptions
        {
            PakOpenOptions = pakOptions,
            JsonMerge = !CliArgs.HasFlag(args, "--last-wins"),
            BuildOptions = buildOptions,
            UnrealPakOptions = ue,
            FilesDirectory = extractDir,
            Log = verbose ? Console.WriteLine : null,
        });

        Console.WriteLine($"extracted -> {extractDir ?? PakMergePaths.ResolveFilesDirectory(output, paks)}");
        Console.WriteLine($"Merged {paks.Count} pak(s) -> {result.OutputPath} ({result.FileCount} files, {result.TotalBytes} bytes)");
        if (!CliArgs.HasFlag(args, "--last-wins"))
            Console.WriteLine("JSON collisions merged in EXTRACTED-MOD/FILES (UE Rows union). Use --last-wins to overwrite.");
        return 0;
    }

    private static List<string> CollectPositionalPakArgs(string[] args, string outputPath)
    {
        var flagsWithValue = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
        {
            "-o", "--output", "--mount", "--game", "--aes-key", "--mods-dir", "--report", "--extract-dir",
        };

        var cfg = UToolConfig.Load();
        var gameId = CliArgs.GetArg(args, "--game");
        var paks = new List<string>();
        for (var i = 0; i < args.Length; i++)
        {
            var arg = args[i];
            if (arg.StartsWith('-'))
            {
                if (flagsWithValue.Contains(arg) && i + 1 < args.Length)
                    i++;
                continue;
            }

            if (string.Equals(arg, outputPath, StringComparison.OrdinalIgnoreCase))
                continue;

            if (Directory.Exists(arg))
                paks.AddRange(ResolveMergePakPaths(arg, cfg, gameId, outputPath));
            else if (File.Exists(arg) && arg.EndsWith(".pak", StringComparison.OrdinalIgnoreCase))
                paks.Add(Path.GetFullPath(arg));
            else
                throw new FileNotFoundException($"Pak not found: {arg}");
        }

        var filtered = PakPathResolver.FilterMergeInputs(paks, outputPath).ToList();
        if (filtered.Count < paks.Count)
            Console.Error.WriteLine($"merge: skipped {paks.Count - filtered.Count} pak(s) (empty or same as -o output).");

        return filtered;
    }

    private static int MergeBuild(string[] args)
    {
        var output = CliArgs.GetArgAny(args, "-o", "--output");
        if (output is null)
            return Missing("merge-build <pak1> <pak2> [...] -o <out.pak>");

        var cfg = UToolConfig.Load();
        var gameId = CliArgs.GetArg(args, "--game");
        var modsDir = CliArgs.GetArg(args, "--mods-dir");
        var paks = CollectPositionalPakArgs(args, output);
        if (paks.Count == 0 && !string.IsNullOrWhiteSpace(modsDir) && Directory.Exists(modsDir))
            paks = ModsPakResolver.ResolveFromModsDirectory(modsDir).Select(m => m.PakPath).ToList();

        if (paks.Count == 0)
            return Missing("merge-build <pak1> <pak2> [...] -o <out.pak>");

        modsDir ??= args.FirstOrDefault(a =>
            !a.StartsWith('-')
            && !string.Equals(a, output, StringComparison.OrdinalIgnoreCase)
            && Directory.Exists(a)
            && ModDiscovery.FindModRoots(a).Any());

        if (!string.IsNullOrWhiteSpace(modsDir) && Directory.Exists(modsDir))
        {
            var order = ModLoadOrderResolver.Resolve(
                ModDiscovery.FindModRoots(modsDir)
                    .Select(r => ModPackageCli.TryLoad(r))
                    .Where(p => p is not null)
                    .Cast<ModPackage>()
                    .ToList());

            var orderedPakPaths = new List<string>();
            foreach (var mod in order.OrderedMods)
            {
                var refs = ModsPakResolver.ResolveFromModsDirectory(modsDir)
                    .Where(m => string.Equals(m.ModId, mod.Manifest.Id, StringComparison.OrdinalIgnoreCase));
                orderedPakPaths.AddRange(refs.Select(r => r.PakPath));
            }

            if (orderedPakPaths.Count > 0)
            {
                var extras = paks.Where(p => !orderedPakPaths.Contains(p, StringComparer.OrdinalIgnoreCase));
                paks = orderedPakPaths.Concat(extras).Distinct(StringComparer.OrdinalIgnoreCase).ToList();
            }

            foreach (var issue in order.Issues)
                Console.Error.WriteLine($"[{issue.Severity}] {issue.ModId}: {issue.Message}");
        }

        var reportDir = CliArgs.GetArg(args, "--report");
        var pakOptions = ResolvePakOpenOptions(args, cfg, gameId);
        var ue = UnrealPakToolchain.ToOptions(cfg.ResolveUnrealPakToolchain(), pakOptions?.AesKey);
        var extractDir = CliArgs.GetArg(args, "--extract-dir");
        var verbose = CliArgs.IsVerbose(args);
        var result = PakMergePipeline.MergeBuild(new PakMergeBuildOptions
        {
            PakPathsInOrder = paks,
            OutputPakPath = output,
            PakOpenOptions = pakOptions,
            BuildOptions = CliArgs.GetArg(args, "--mount") is { } mount
                ? new PakBuildOptions { MountPoint = mount }
                : null,
            ConflictReportDirectory = reportDir,
            FilesDirectory = extractDir,
            JsonMerge = !CliArgs.HasFlag(args, "--last-wins"),
            UnrealPakOptions = ue,
            Log = verbose ? Console.WriteLine : null,
        });

        Console.WriteLine($"extracted -> {result.ExtractedFilesDirectory}");
        Console.WriteLine($"merge-build -> {result.Build.OutputPath} ({result.Build.FileCount} files, {result.Build.TotalBytes} bytes)");
        Console.WriteLine($"json merges: {result.JsonMergeCount}, pak path overlaps: {result.Overlaps.Count}");
        Console.WriteLine($"source paks: {paks.Count}");
        foreach (var r in result.JsonReports.Where(x => x.TotalConflicts > 0))
            Console.WriteLine($"  conflicts: {r.AssetLabel} ({r.TotalConflicts} property)");

        if (result.Overlaps.Any(c => !c.IdenticalContent))
            return 2;
        return 0;
    }
}
