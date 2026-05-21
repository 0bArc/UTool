using System.Text;
using CsStratware.Core.Models;
using CsStratware.Infrastructure.Logging;
using CsStratware.Infrastructure.Operations;
using CsStratware.ModLoader;
using CsStratware.ModLoader.Merge;
using CsStratware.Pak;

namespace CsStratware.Cli;

internal static class PakCommands
{
    public static void PrintUsage()
    {
        Console.WriteLine("""
            pak commands:
              csmanager pak list <file.pak>
              csmanager pak find <paks-dir|@paks> <needle> [--game <gameId>] [--path-only] [--grep] [--extracted dir] [--aes-key hex]
              csmanager pak build <content-dir> -o <out.pak> [--mount ../../../Game/]
              csmanager pak build-mod <mod-dir> [-o <out.pak>] [--mount ...] [--prepare] [--ue-pack]
              csmanager pak check <mods-dir|paks-dir|@paks> [--game <id>] [--aes-key hex]
              csmanager pak merge <pak1> <pak2> [...] -o <out.pak> [--last-wins] [--mount ...] [--game <id>]
              csmanager pak merge-build <pak1> <pak2> [...] -o <out.pak> [--mods-dir dir] [--report dir] [--extract-dir path] [--game <id>] [-v|--verbose]
              csmanager pak patch <base.pak> <overlay-dir> -o <out.pak>
              csmanager pak extract <file.pak> <out-dir>
              csmanager pak search <file-or-dir> [--pattern text] [--ext .json] [--max 100]
              csmanager pak cat <file.pak> <entry-path> [-o out.json]
              csmanager pak grep <file-or-dir> <needle> [--max N]
              csmanager pak data list <pak|paks-dir|@paks> [--pattern *Recipe*] [--ext .json,.ini] [--game <id>] [--aes-key hex|base64]
              csmanager pak data pull <pak|paks-dir|@paks> <out-dir> [--pattern ...] [--ext ...] [--game <id>] [--aes-key ...] [--no-ue-fallback]
              csmanager pak ue extract <pak|paks-dir|@paks> <out-dir> [--filter *Recipe*] [--game <id>] [--aes-key ...]
              csmanager pak ue pack <content-dir> -o <out.pak> [--mount <ue-mount>] [--game <gameId>] [-compress]
            """);
    }

    public static int Run(string[] args)
    {
        if (args.Length == 0 || args[0] is "-h" or "--help" or "help")
        {
            PrintUsage();
            return 0;
        }

        var sub = args[0].ToLowerInvariant();
        if (sub == "ue" && args.Length > 1)
        {
            var ueSub = args[1].ToLowerInvariant();
            var ueArgs = args[2..];
            try
            {
                return ueSub switch
                {
                    "extract" => UeExtract(ueArgs),
                    "pack" => UePack(ueArgs),
                    _ => Unknown($"ue {ueSub}"),
                };
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"pak error: {ex.Message}");
                return 1;
            }
        }

        if (sub == "data" && args.Length > 1)
        {
            var dataSub = args[1].ToLowerInvariant();
            var dataArgs = args[2..];
            try
            {
                return dataSub switch
                {
                    "list" => DataList(dataArgs),
                    "pull" => DataPull(dataArgs),
                    _ => Unknown($"data {dataSub}"),
                };
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"pak error: {ex.Message}");
                return 1;
            }
        }

        try
        {
            return sub switch
            {
                "list" => List(args),
                "find" => Find(args),
                "build" => Build(args),
                "build-mod" => BuildMod(args),
                "check" => Check(args[1..]),
                "merge" => Merge(args[1..]),
                "merge-build" => MergeBuild(args[1..]),
                "patch" => Patch(args),
                "extract" => Extract(args),
                "search" => Search(args),
                "cat" => Cat(args),
                "grep" => Grep(args),
                _ => Unknown(sub),
            };
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"pak error: {ex.Message}");
            return 1;
        }
    }

    private static int Unknown(string sub)
    {
        Console.Error.WriteLine($"Unknown pak command: {sub}");
        PrintUsage();
        return 1;
    }

    private static int List(string[] args)
    {
        if (args.Length < 2)
            return Missing("list <file.pak>");

        var archive = PakArchiveCache.Open(args[1], ResolvePakOpenOptions(args));
        Console.WriteLine($"{archive.FilePath}");
        Console.WriteLine($"mount: {archive.MountPoint}");
        Console.WriteLine($"version: {archive.Footer.Version}");
        Console.WriteLine($"files: {archive.Entries.Count}");

        foreach (var entry in archive.Entries.Values.OrderBy(e => e.Path, StringComparer.OrdinalIgnoreCase))
        {
            var flags = entry.IsEncrypted ? " enc" : string.Empty;
            var comp = entry.IsCompressed ? $" comp#{entry.CompressionMethodIndex}" : string.Empty;
            Console.WriteLine($"  {entry.Path} ({entry.UncompressedSize} bytes){comp}{flags}");
        }

        return 0;
    }

    private static int Build(string[] args)
    {
        if (args.Length < 2)
            return Missing("build <content-dir> -o <out.pak>");

        var contentDir = args[1];
        var output = GetArg(args, "-o") ?? GetArg(args, "--output");
        if (output is null)
            return Missing("build <content-dir> -o <out.pak>");

        var mount = GetArg(args, "--mount") ?? "../../../YourGame/";
        var result = PakBuilder.BuildFromDirectory(contentDir, output, new PakBuildOptions { MountPoint = mount });
        Console.WriteLine($"Built {result.FileCount} file(s) -> {result.OutputPath} ({result.TotalBytes} bytes)");
        return 0;
    }

    private static int BuildMod(string[] args)
    {
        if (args.Length < 2)
            return Missing("build-mod <mod-dir>");

        var modDir = args[1];
        var manifestPath = Path.Combine(modDir, ModManifestReader.ManifestFileName);
        if (!File.Exists(manifestPath))
        {
            Console.Error.WriteLine($"No mod.json in {modDir}");
            return 1;
        }

        var package = ModDiscovery.TryLoadPackageAsync(modDir).GetAwaiter().GetResult();
        if (package is null)
        {
            Console.Error.WriteLine("Failed to load mod package.");
            return 1;
        }

        var cfg = StratwareConfig.Load(modDir);
        var output = GetArg(args, "-o")
            ?? GetArg(args, "--output")
            ?? package.Manifest.Pak?.Output
            ?? Path.Combine(modDir, "dist", $"{SanitizePakName(package.Manifest.Id)}_P.pak");
        if (!Path.IsPathRooted(output))
            output = Path.Combine(modDir, output);

        var gameId = package.Manifest.Target?.GameId;
        var mount = GetArg(args, "--mount")
            ?? package.Manifest.Pak?.MountPoint
            ?? cfg.ResolveMountPoint(gameId);
        if (string.IsNullOrWhiteSpace(mount))
        {
            Console.Error.WriteLine("Mount point required: mod.json pak.mountPoint, --mount, or defaultMountPoint in csstratware.json.");
            return 1;
        }

        var useUePack = HasFlag(args, "--ue-pack")
            || package.Manifest.Pak?.UseUnrealPak == true
            || !string.IsNullOrWhiteSpace(package.Manifest.Pak?.SourcePak);

        var hasCode = ModCodeCompiler.HasCodeProject(package);
        var hasCurves = Directory.Exists(Path.Combine(modDir, package.Manifest.CurvePatchesDir ?? "curves"))
            && Directory.EnumerateFiles(Path.Combine(modDir, package.Manifest.CurvePatchesDir ?? "curves"), "*.curve.json").Any();
        var shouldPrepare = HasFlag(args, "--prepare")
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

            var sourcePak = cfg.ResolveSourcePak(package.Manifest.Pak?.SourcePak, gameId);
            var curveSource = package.Manifest.Pak?.CurveSourcePak ?? "@paks";
            var curveSourcePaks = cfg.ResolveSourcePakPaths(curveSource, gameId);

            var toolchain = cfg.ResolveUnrealPakToolchain();
            var opCtx = CreateOperationContext(args);
            var prepared = ModAssetPreparer.Prepare(package, new ModPrepareOptions
            {
                SourcePakPath = sourcePak,
                CurveSourcePakPaths = curveSourcePaks,
                PakAesKey = cfg.ResolvePakAesKey(gameId),
                UnrealPakOptions = UnrealPakToolchain.ToOptions(toolchain, cfg.ResolvePakAesKey(gameId)),
                ExtractedDir = cfg.ResolveExtractedDir(),
                UnrealPakExecutable = toolchain.Executable,
                CompiledAssemblyPath = compiledAssembly,
                PlayerDataRoot = cfg.ResolvePlayerDataDir(package.Manifest.Target?.GameId),
                ForceExtract = HasFlag(args, "--force-extract"),
                Operation = opCtx,
                SkipIfUpToDate = !HasFlag(args, "--force-extract"),
            });
            contentRoot = ModBuildContent.MergeForPack(package, prepared.PreparedContentDir);
            foreach (var file in prepared.PreparedFiles)
                Console.WriteLine($"prepared: {file} ({new FileInfo(file).Length} bytes)");
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
        Directory.CreateDirectory(Path.GetDirectoryName(output)!);

        if (useUePack)
        {
            var ue = UnrealPakToolchain.ToOptions(cfg.ResolveUnrealPakToolchain());
            UnrealPakRunner.PackDirectory(contentRoot, output, mount, HasFlag(args, "-compress"), ue);
            Console.WriteLine($"Built mod pak (UnrealPak): {Path.GetFullPath(output)}");
            return 0;
        }

        var options = new PakBuildOptions { MountPoint = mount };
        var result = ModPakBuilder.BuildModPak(package, output, options);
        Console.WriteLine($"Built mod pak: {result.OutputPath} ({result.FileCount} files, {result.TotalBytes} bytes)");
        return 0;
    }

    private static string SanitizePakName(string id) =>
        id.Replace('.', '-').Replace(' ', '-');

    private static int Check(string[] args)
    {
        if (args.Length < 1)
            return Missing("check <mods-dir|paks-dir|@paks>");

        var cfg = StratwareConfig.Load();
        var gameId = GetArg(args, "--game");
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

    private static IReadOnlyList<string> ResolveCheckPakPaths(string target, StratwareConfig cfg, string? gameId)
    {
        if (Directory.Exists(target))
            return ResolveMergePakPaths(target, cfg, gameId);

        return ResolvePakSources(target, cfg, gameId);
    }

    private static IReadOnlyList<string> ResolveMergePakPaths(
        string target,
        StratwareConfig cfg,
        string? gameId,
        string? excludeOutputPak = null)
    {
        if (StratwareConfig.IsPaksDirAlias(target) || StratwareConfig.IsDataPakAlias(target))
            return PakPathResolver.FilterMergeInputs(ResolvePakSources(target, cfg, gameId), excludeOutputPak);

        return PakPathResolver.ResolveForMerge(target, excludeOutputPak);
    }

    private static int Merge(string[] args)
    {
        var output = GetArg(args, "-o") ?? GetArg(args, "--output");
        if (output is null)
            return Missing("merge <pak1> <pak2> [...] -o <out.pak>");

        var cfg = StratwareConfig.Load();
        var gameId = GetArg(args, "--game");
        var paks = CollectPositionalPakArgs(args, output);
        if (paks.Count == 0)
            return Missing("merge <pak1> <pak2> [...] -o <out.pak>");

        var mount = GetArg(args, "--mount");
        var buildOptions = mount is null ? null : new PakBuildOptions { MountPoint = mount };
        var pakOptions = ResolvePakOpenOptions(args, cfg, gameId);
        var ue = UnrealPakToolchain.ToOptions(cfg.ResolveUnrealPakToolchain(), pakOptions?.AesKey);
        var extractDir = GetArg(args, "--extract-dir");
        var verbose = HasFlag(args, "--verbose") || HasFlag(args, "-v");
        var result = PakMerger.Merge(paks, output, new PakMergeOptions
        {
            PakOpenOptions = pakOptions,
            JsonMerge = !HasFlag(args, "--last-wins"),
            BuildOptions = buildOptions,
            UnrealPakOptions = ue,
            FilesDirectory = extractDir,
            Log = verbose ? Console.WriteLine : null,
        });

        Console.WriteLine($"extracted -> {extractDir ?? PakMergePaths.ResolveFilesDirectory(output, paks)}");
        Console.WriteLine($"Merged {paks.Count} pak(s) -> {result.OutputPath} ({result.FileCount} files, {result.TotalBytes} bytes)");
        if (!HasFlag(args, "--last-wins"))
            Console.WriteLine("JSON collisions merged in EXTRACTED-MOD/FILES (UE Rows union). Use --last-wins to overwrite.");
        return 0;
    }

    private static List<string> CollectPositionalPakArgs(string[] args, string outputPath)
    {
        var flagsWithValue = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
        {
            "-o", "--output", "--mount", "--game", "--aes-key", "--mods-dir", "--report", "--extract-dir",
        };

        var cfg = StratwareConfig.Load();
        var gameId = GetArg(args, "--game");
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
        var output = GetArg(args, "-o") ?? GetArg(args, "--output");
        if (output is null)
            return Missing("merge-build <pak1> <pak2> [...] -o <out.pak>");

        var cfg = StratwareConfig.Load();
        var gameId = GetArg(args, "--game");
        var modsDir = GetArg(args, "--mods-dir");
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
                    .Select(r => ModDiscovery.TryLoadPackageAsync(r).GetAwaiter().GetResult())
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

        var reportDir = GetArg(args, "--report");
        var pakOptions = ResolvePakOpenOptions(args, cfg, gameId);
        var ue = UnrealPakToolchain.ToOptions(cfg.ResolveUnrealPakToolchain(), pakOptions?.AesKey);
        var extractDir = GetArg(args, "--extract-dir");
        var verbose = HasFlag(args, "--verbose") || HasFlag(args, "-v");
        var result = PakMergePipeline.MergeBuild(new PakMergeBuildOptions
        {
            PakPathsInOrder = paks,
            OutputPakPath = output,
            PakOpenOptions = pakOptions,
            BuildOptions = GetArg(args, "--mount") is { } mount
                ? new PakBuildOptions { MountPoint = mount }
                : null,
            ConflictReportDirectory = reportDir,
            FilesDirectory = extractDir,
            JsonMerge = !HasFlag(args, "--last-wins"),
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

    private static int Patch(string[] args)
    {
        if (args.Length < 3)
            return Missing("patch <base.pak> <overlay-dir> -o <out.pak>");

        var output = GetArg(args, "-o") ?? GetArg(args, "--output");
        if (output is null)
            return Missing("patch <base.pak> <overlay-dir> -o <out.pak>");

        var mount = GetArg(args, "--mount");
        var options = mount is null ? null : new PakBuildOptions { MountPoint = mount };
        var result = PakPatcher.Patch(args[1], args[2], output, options);
        Console.WriteLine($"Patched pak -> {result.OutputPath} ({result.FileCount} files)");
        return 0;
    }

    private static int Extract(string[] args)
    {
        if (args.Length < 3)
            return Missing("extract <file.pak> <out-dir>");

        var pakOptions = ResolvePakOpenOptions(args);
        var archive = PakArchiveCache.Open(args[1], pakOptions);
        PakEntryExtractor.ExtractToDirectory(archive, args[2], aesKey: pakOptions?.AesKey);
        Console.WriteLine($"Extracted {archive.Entries.Count} entries to {args[2]}");
        return 0;
    }

    private static int Search(string[] args)
    {
        if (args.Length < 2)
            return Missing("search <file.pak|paks-dir> [--pattern text] [--ext .json] [--max N]");

        var target = args[1];
        var pattern = GetArg(args, "--pattern") ?? GetArg(args, "-p");
        var extArg = GetArg(args, "--ext") ?? GetArg(args, "-e");
        var extensions = string.IsNullOrWhiteSpace(extArg)
            ? Array.Empty<string>()
            : extArg.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        var max = 500;
        if (int.TryParse(GetArg(args, "--max"), out var parsedMax) && parsedMax > 0)
            max = parsedMax;

        var options = new PakSearchOptions
        {
            Pattern = pattern,
            Extensions = extensions,
            MaxResults = max,
        };

        var matches = Directory.Exists(target)
            ? PakArchiveSearch.SearchDirectory(target, options)
            : PakArchiveSearch.SearchFile(PakArchiveCache.Open(target, ResolvePakOpenOptions(args)), options);

        foreach (var match in matches)
            Console.WriteLine($"{match.PakPath} :: {match.Entry.Path} ({match.Entry.UncompressedSize} bytes)");

        Console.WriteLine($"matches: {matches.Count}");
        return 0;
    }

    private static int Cat(string[] args)
    {
        if (args.Length < 3)
            return Missing("cat <file.pak> <entry-path> [-o outfile]");

        var pakPath = args[1];
        var entryPath = args[2];
        var output = GetArg(args, "-o") ?? GetArg(args, "--output");

        var pakOptions = ResolvePakOpenOptions(args);
        var archive = PakArchiveCache.Open(pakPath, pakOptions);
        if (!archive.Entries.TryGetValue(entryPath, out var entry))
        {
            var alt = archive.Entries.Keys.FirstOrDefault(k =>
                k.EndsWith(entryPath, StringComparison.OrdinalIgnoreCase)
                || k.Contains(entryPath, StringComparison.OrdinalIgnoreCase));
            if (alt is null)
            {
                Console.Error.WriteLine($"Entry not found: {entryPath}");
                return 1;
            }

            entry = archive.Entries[alt];
        }

        using var stream = File.OpenRead(pakPath);
        var data = PakEntryExtractor.ReadEntry(stream, entry, archive.Footer, pakOptions?.AesKey);
        if (output is null)
        {
            Console.Write(Encoding.UTF8.GetString(data));
        }
        else
        {
            Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(output))!);
            File.WriteAllBytes(output, data);
            Console.WriteLine($"Wrote {data.Length} bytes -> {output}");
        }

        return 0;
    }

    private static int Find(string[] args)
    {
        if (args.Length < 3)
            return Missing("find <paks-dir|pak> <needle> [--path-only] [--extracted dir] [--max N]");

        var cfg = StratwareConfig.Load();
        var target = args[1];
        var gameId = GetArg(args, "--game");
        if (StratwareConfig.IsPaksDirAlias(target))
        {
            var paks = cfg.ResolvePaksDir(gameId);
            if (string.IsNullOrWhiteSpace(paks))
            {
                Console.Error.WriteLine("gamePaksDir not set in csstratware.json (or games.<gameId>.paksDir).");
                return 1;
            }

            target = paks;
        }

        var needle = args[2];
        var max = 30;
        if (int.TryParse(GetArg(args, "--max"), out var parsedMax) && parsedMax > 0)
            max = parsedMax;

        var extracted = GetArg(args, "--extracted") ?? cfg.ResolveExtractedDir();
        var hits = PakFind.Find(target, needle, new PakFindOptions
        {
            MaxResults = max,
            PathOnly = HasFlag(args, "--path-only"),
            GrepContent = HasFlag(args, "--grep"),
            ExtractedDir = extracted,
            PakOpenOptions = ResolvePakOpenOptions(args),
        });

        foreach (var hit in hits)
        {
            var line = hit.Kind switch
            {
                PakFindHitKind.Path => $"[path] {hit.PakPath} :: {hit.EntryPath} ({hit.Size} bytes)",
                PakFindHitKind.Content => $"[content] {hit.PakPath} :: {hit.EntryPath} @0x{hit.Offset:X}",
                PakFindHitKind.Disk => $"[disk] {hit.FilePath}",
                _ => hit.ToString() ?? "",
            };
            Console.WriteLine(line);
        }

        Console.WriteLine($"hits: {hits.Count}");
        if (hits.Count == 0)
        {
            Console.WriteLine("tip: JSON/ini often readable via native reader; .uasset may need UnrealPak/FModel:");
            Console.WriteLine("  pak data pull <paks-dir|@paks> ./extracted --pattern *Recipe* --ext .json");
            Console.WriteLine("  pak ue extract <pak> ./extracted -filter *Processor*");
            Console.WriteLine("  pak find ./extracted RequiredMillijoules");
        }

        return 0;
    }

    private static int DataList(string[] args)
    {
        if (args.Length < 1)
            return Missing("data list <pak|paks-dir|@paks> [--pattern text] [--ext .json] [--game <id>]");

        var cfg = StratwareConfig.Load();
        var gameId = GetArg(args, "--game");
        var paks = ResolvePakSources(args[0], cfg, gameId);
        var options = BuildDataPullOptions(args, cfg, gameId);
        var matches = PakDataPuller.List(paks, options);
        foreach (var match in matches)
            Console.WriteLine($"{match.PakPath} :: {match.Entry.Path} ({match.Entry.UncompressedSize} bytes)");

        Console.WriteLine($"entries: {matches.Count} in {paks.Count} pak(s)");
        return 0;
    }

    private static int DataPull(string[] args)
    {
        if (args.Length < 2)
            return Missing("data pull <pak|paks-dir|@paks> <out-dir> [--pattern ...] [--ext .json,.ini] [--game <id>]");

        var cfg = StratwareConfig.Load();
        var gameId = GetArg(args, "--game");
        var paks = ResolvePakSources(args[0], cfg, gameId);
        var outDir = args[1];
        var options = BuildDataPullOptions(args, cfg, gameId);
        var aesKey = options.PakOpenOptions?.AesKey;
        options = new PakDataPullOptions
        {
            Pattern = options.Pattern,
            Extensions = options.Extensions,
            MaxFiles = options.MaxFiles,
            PakOpenOptions = options.PakOpenOptions,
            UnrealPakOptions = UnrealPakToolchain.ToOptions(cfg.ResolveUnrealPakToolchain(), aesKey),
            UnrealPakFallback = !HasFlag(args, "--no-ue-fallback"),
            Log = HasFlag(args, "--verbose") || HasFlag(args, "-v") ? Console.WriteLine : null,
        };

        var result = PakDataPuller.Pull(paks, outDir, options);
        Console.WriteLine($"data pull -> {result.OutputDirectory}");
        Console.WriteLine($"written: {result.Written} (unrealpak: {result.UnrealPakExtracted}, deferred: {result.SkippedEncrypted})");
        if (result.Written == 0 && aesKey is null)
            Console.WriteLine("hint: encrypted paks need pakAesKey in csstratware.json, --aes-key, or PAK_AES_KEY.");
        return 0;
    }

    private static PakDataPullOptions BuildDataPullOptions(
        string[] args,
        StratwareConfig? cfg = null,
        string? gameId = null)
    {
        cfg ??= StratwareConfig.Load();
        var extArg = GetArg(args, "--ext") ?? GetArg(args, "-e");
        var extensions = string.IsNullOrWhiteSpace(extArg)
            ? new PakDataPullOptions().Extensions
            : extArg.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);

        var max = 0;
        if (int.TryParse(GetArg(args, "--max"), out var parsedMax) && parsedMax > 0)
            max = parsedMax;

        return new PakDataPullOptions
        {
            Pattern = GetArg(args, "--pattern") ?? GetArg(args, "-p"),
            Extensions = extensions,
            MaxFiles = max,
            PakOpenOptions = ResolvePakOpenOptions(args, cfg, gameId),
        };
    }

    private static int UeExtract(string[] args)
    {
        if (args.Length < 2)
            return Missing("ue extract <pak|paks-dir|@paks> <out-dir> [--filter wildcard] [--game <id>]");

        var cfg = StratwareConfig.Load();
        var gameId = GetArg(args, "--game");
        var paks = ResolvePakSources(args[0], cfg, gameId);
        var outDir = args[1];
        var filter = GetArg(args, "--filter") ?? GetArg(args, "-filter") ?? GetArg(args, "-f");
        var pakOptions = ResolvePakOpenOptions(args, cfg, gameId);
        var ue = UnrealPakToolchain.ToOptions(cfg.ResolveUnrealPakToolchain(), pakOptions?.AesKey);

        Directory.CreateDirectory(outDir);
        foreach (var pak in paks)
        {
            var dest = paks.Count == 1
                ? outDir
                : Path.Combine(outDir, Path.GetFileNameWithoutExtension(pak));
            Directory.CreateDirectory(dest);
            UnrealPakRunner.Extract(pak, dest, filter, ue);
            Console.WriteLine($"UnrealPak extract: {pak} -> {Path.GetFullPath(dest)}");
        }

        Console.WriteLine($"UnrealPak extract done ({paks.Count} pak(s)) -> {Path.GetFullPath(outDir)}");
        return 0;
    }

    private static IReadOnlyList<string> ResolvePakSources(string target, StratwareConfig cfg, string? gameId)
    {
        if (StratwareConfig.IsPaksDirAlias(target))
        {
            var paksDir = cfg.ResolvePaksDir(gameId);
            if (string.IsNullOrWhiteSpace(paksDir))
            {
                throw new InvalidOperationException(
                    "gamePaksDir not set in csstratware.json (or games.<gameId>.paksDir). Use --game or set gamePaksDir.");
            }

            return PakPathResolver.Resolve(paksDir);
        }

        if (StratwareConfig.IsDataPakAlias(target))
        {
            var dataPak = cfg.ResolveDataPak(gameId);
            return PakPathResolver.Resolve(dataPak);
        }

        return PakPathResolver.Resolve(target);
    }

    private static int UePack(string[] args)
    {
        if (args.Length < 2)
            return Missing("ue pack <content-dir> -o <out.pak> [--mount <ue-mount>] [--game <gameId>]");

        var content = args[0];
        var output = GetArg(args, "-o") ?? GetArg(args, "--output");
        if (output is null)
            return Missing("ue pack <content-dir> -o <out.pak>");

        var cfg = StratwareConfig.Load();
        var mount = GetArg(args, "--mount") ?? cfg.ResolveMountPoint(GetArg(args, "--game"));
        if (string.IsNullOrWhiteSpace(mount))
        {
            Console.Error.WriteLine("Mount point required: --mount or defaultMountPoint in csstratware.json.");
            return 1;
        }
        var ue = UnrealPakToolchain.ToOptions(cfg.ResolveUnrealPakToolchain());
        UnrealPakRunner.PackDirectory(content, output, mount, HasFlag(args, "-compress"), ue);
        Console.WriteLine($"UnrealPak pack -> {Path.GetFullPath(output)}");
        return 0;
    }

    private static int Grep(string[] args)
    {
        if (args.Length < 3)
            return Missing("grep <file.pak|paks-dir> <needle> [--max N]");

        var target = args[1];
        var needle = args[2];
        var max = 50;
        if (int.TryParse(GetArg(args, "--max"), out var parsedMax) && parsedMax > 0)
            max = parsedMax;

        var matches = Directory.Exists(target)
            ? PakContentSearch.GrepDirectory(target, needle, max)
            : PakContentSearch.GrepFile(PakArchiveCache.Open(target, ResolvePakOpenOptions(args)), needle, max);

        foreach (var match in matches)
            Console.WriteLine($"{match.PakPath} :: {match.EntryPath} @0x{match.Offset:X} ({match.UncompressedSize} bytes)");

        Console.WriteLine($"matches: {matches.Count}");
        return 0;
    }

    private static int Missing(string usage)
    {
        Console.Error.WriteLine($"Usage: {usage}");
        PrintUsage();
        return 1;
    }

    private static string? GetArg(string[] args, string flag)
    {
        for (var i = 0; i < args.Length - 1; i++)
        {
            if (string.Equals(args[i], flag, StringComparison.OrdinalIgnoreCase))
                return args[i + 1];
        }

        return null;
    }

    private static bool HasFlag(string[] args, string flag) =>
        args.Any(a => string.Equals(a, flag, StringComparison.OrdinalIgnoreCase));

    private static PakOpenOptions? ResolvePakOpenOptions(
        string[] args,
        StratwareConfig? cfg = null,
        string? gameId = null)
    {
        cfg ??= StratwareConfig.Load();
        gameId ??= GetArg(args, "--game");
        var bytes = PakOpenOptions.ParseAesKey(GetArg(args, "--aes-key"))
            ?? PakOpenOptions.ParseAesKey(Environment.GetEnvironmentVariable("PAK_AES_KEY"))
            ?? cfg.ResolvePakAesKey(gameId);
        return bytes is null ? null : new PakOpenOptions { AesKey = bytes };
    }

    private static OperationContext CreateOperationContext(string[] args)
    {
        if (HasFlag(args, "--verbose") || HasFlag(args, "-v"))
            StratwareLog.MinimumLevel = LogLevel.Debug;

        var progress = HasFlag(args, "--progress")
            ? new Progress<OperationProgress>(p =>
            {
                var total = p.Total is int t ? $"/{t}" : "";
                Console.Error.WriteLine($"[{p.Current}{total}] {p.Message}");
            })
            : null;

        return new OperationContext { Progress = progress };
    }
}
