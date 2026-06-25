using UTool.ModLoader;
using UTool.Pak;

namespace UTool.Cli;

internal static partial class PakCommands
{
    private static int Build(string[] args)
    {
        if (args.Length < 2)
            return Missing("build <content-dir> -o <out.pak>");

        var contentDir = args[1];
        var output = CliArgs.GetArgAny(args, "-o", "--output");
        if (output is null)
            return Missing("build <content-dir> -o <out.pak>");

        var mount = CliArgs.GetArg(args, "--mount") ?? "../../../YourGame/";
        var result = PakBuilder.BuildFromDirectory(contentDir, output, new PakBuildOptions { MountPoint = mount });
        Console.WriteLine($"Built {result.FileCount} file(s) -> {result.OutputPath} ({result.TotalBytes} bytes)");
        return 0;
    }

    private static int BuildMod(string[] args)
    {
        if (args.Length < 2)
            return Missing("build-mod <mod-dir>");

        EnsureHostSupportsCurvePatches();

        var modDir = args[1];
        var manifestPath = Path.Combine(modDir, ModManifestReader.ManifestFileName);
        if (!File.Exists(manifestPath))
        {
            Console.Error.WriteLine($"No mod.json in {modDir}");
            return 1;
        }

        var package = ModPackageCli.TryLoad(modDir);
        if (package is null)
        {
            Console.Error.WriteLine("Failed to load mod package.");
            return 1;
        }

        var cfg = UToolConfig.Load(modDir);
        var output = CliArgs.GetArgAny(args, "-o", "--output")
            ?? package.Manifest.Pak?.Output
            ?? Path.Combine(modDir, "dist", $"{SanitizePakName(package.Manifest.Id)}_P.pak");
        if (!Path.IsPathRooted(output))
            output = Path.Combine(modDir, output);

        var gameId = package.Manifest.Target?.GameId;
        var mount = CliArgs.GetArg(args, "--mount")
            ?? package.Manifest.Pak?.MountPoint
            ?? cfg.ResolveMountPoint(gameId);
        if (string.IsNullOrWhiteSpace(mount))
        {
            Console.Error.WriteLine("Mount point required: mod.json pak.mountPoint, --mount, or defaultMountPoint in utool.json.");
            return 1;
        }

        var useUePack = CliArgs.HasFlag(args, "--ue-pack")
            || package.Manifest.Pak?.UseUnrealPak == true
            || !string.IsNullOrWhiteSpace(package.Manifest.Pak?.SourcePak);

        ModCodeProjectScaffold.EnsureProject(package);
        var hasCode = ModCodeCompiler.HasCodeProject(package);
        var curvesDir = Path.Combine(modDir, package.Manifest.CurvePatchesDir ?? "curves");
        var hasJsonCurves = Directory.Exists(curvesDir)
            && Directory.EnumerateFiles(curvesDir, "*.curve.json").Any();
        var hasCurves = hasJsonCurves || hasCode;
        var shouldPrepare = CliArgs.HasFlag(args, "--prepare")
            || package.Manifest.PatchFiles.Count > 0
            || hasCode
            || hasCurves;

        string contentRoot;
        if (shouldPrepare && (package.Manifest.PatchFiles.Count > 0 || hasCode || hasCurves))
        {
            string? compiledAssembly = null;
            if (hasCode)
            {
                var compiled = ModCodeCompiler.Compile(package);
                compiledAssembly = compiled.AssemblyPath;
                Console.WriteLine($"compiled: {compiledAssembly}");
            }

            var forceExtract = CliArgs.HasFlag(args, "--force-extract");
            var prepared = ModPreparePipeline.Prepare(package, cfg, new ModPrepareCliOptions
            {
                GameId = gameId,
                CompiledAssemblyPath = compiledAssembly,
                PreserveSourcePaths = IsDataRootMount(mount),
                ForceExtract = forceExtract,
                SkipIfUpToDate = !forceExtract,
                Operation = CreateOperationContext(args),
            });
            contentRoot = ModBuildContent.MergeForPack(package, prepared.PreparedContentDir);
            ModPreparePipeline.WritePreparedFiles(prepared.PreparedFiles);
            ModCodePatchRunner.UnloadMod(package.Manifest.Id);
        }
        else
        {
            contentRoot = package.Manifest.ContentRoots
                .Select(r => Path.Combine(modDir, r))
                .FirstOrDefault(Directory.Exists)
                ?? "";
        }

        if (string.IsNullOrEmpty(contentRoot) || !Directory.Exists(contentRoot))
        {
            Console.Error.WriteLine("No content to pack. Use patchFiles, code/*.csproj, or content/.");
            return 1;
        }

        output = Path.GetFullPath(output);
        output = EnsurePatchPakName(output, gameId);
        Directory.CreateDirectory(Path.GetDirectoryName(output)!);

        if (useUePack)
        {
            var ue = UnrealPakToolchain.ToOptions(cfg.ResolveUnrealPakToolchain());
            UnrealPakRunner.PackDirectory(contentRoot, output, mount, CliArgs.HasFlag(args, "-compress"), ue);
            Console.WriteLine($"Built mod pak (UnrealPak): {Path.GetFullPath(output)}");
            ModBuildCleanup.AfterPack(modDir, package.Manifest.Pak?.KeepCache == true);
            return 0;
        }

        var options = new PakBuildOptions { MountPoint = mount };
        var result = ModPakBuilder.BuildModPak(package, output, options);
        Console.WriteLine($"Built mod pak: {result.OutputPath} ({result.FileCount} files, {result.TotalBytes} bytes)");
        ModBuildCleanup.AfterPack(modDir, package.Manifest.Pak?.KeepCache == true);
        return 0;
    }

    private static string SanitizePakName(string id) =>
        id.Replace('.', '-').Replace(' ', '-');

    private static bool IsDataRootMount(string mount)
    {
        var normalized = mount.Replace('\\', '/').TrimEnd('/');
        return normalized.EndsWith("/Content/Data", StringComparison.OrdinalIgnoreCase);
    }

    private static string EnsurePatchPakName(string output, string? gameId)
    {
        if (!string.Equals(gameId, "Icarus", StringComparison.OrdinalIgnoreCase))
            return output;

        if (!string.Equals(Path.GetExtension(output), ".pak", StringComparison.OrdinalIgnoreCase))
            return output;

        var name = Path.GetFileNameWithoutExtension(output);
        if (name.EndsWith("_P", StringComparison.OrdinalIgnoreCase))
            return output;

        var renamed = Path.Combine(Path.GetDirectoryName(output) ?? "", name + "_P.pak");
        Console.WriteLine($"Icarus patch pak output: {Path.GetFileName(renamed)}");
        return renamed;
    }

    private static int Check(string[] args)
    {
        if (args.Length < 1)
            return Missing("check <mods-dir|paks-dir|@paks>");

        var cfg = UToolConfig.Load();
        var gameId = CliArgs.GetArg(args, "--game");
        var paks = ResolveCheckPakPaths(args[0], cfg, gameId);
        if (paks.Count == 0)
        {
            Console.Error.WriteLine("No .pak files found for check.");
            return 1;
        }

        var modRefs = Directory.Exists(args[0])
            ? ModsPakResolver.ResolveFromModsDirectory(args[0])
            : Array.Empty<ModPakReference>();

        var pakOptions = ResolvePakOpenOptions(args, cfg, gameId);
        var report = PakOverlapChecker.Analyze(paks, pakOptions);

        Console.WriteLine($"checked {paks.Count} pak(s), {report.DistinctPaths} unique path(s)");
        foreach (var pak in paks)
        {
            var mod = modRefs.FirstOrDefault(m => string.Equals(m.PakPath, pak, StringComparison.OrdinalIgnoreCase));
            Console.WriteLine(mod is null ? $"  {pak}" : $"  {pak}  [{mod.ModId}]");
        }

        if (report.Conflicts.Count == 0)
        {
            Console.WriteLine("no path overlaps between paks.");
            return 0;
        }

        foreach (var conflict in report.Conflicts)
        {
            var tag = conflict.IdenticalContent ? "SAME" : "CONFLICT";
            Console.WriteLine($"{tag} {conflict.RelativePath}");
            foreach (var source in conflict.Sources)
            {
                var mod = modRefs.FirstOrDefault(m =>
                    string.Equals(m.PakPath, source.PakPath, StringComparison.OrdinalIgnoreCase));
                var modTag = mod is null ? "" : $" [{mod.ModId}]";
                var hash = source.ContentHash is null ? "" : $" sha256={source.ContentHash[..12]}...";
                Console.WriteLine($"    {Path.GetFileName(source.PakPath)}{modTag} ({source.UncompressedSize} bytes){hash}");
            }

            if (!conflict.IdenticalContent && conflict.RelativePath.EndsWith(".json", StringComparison.OrdinalIgnoreCase))
                Console.WriteLine("    hint: pak merge -o merged.pak unions UE Rows (Name/RowName); last-wins without --json-merge off");
        }

        Console.WriteLine($"overlaps: {report.Conflicts.Count} ({report.Conflicts.Count(c => !c.IdenticalContent)} content conflict(s))");
        return report.HasContentConflicts ? 2 : 0;
    }
}
