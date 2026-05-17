using System.Text;
using CsStratware.Core.Models;
using CsStratware.ModLoader;
using CsStratware.Pak;

namespace CsStratware.Cli;

internal static class PakCommands
{
    public static void PrintUsage()
    {
        Console.WriteLine("""
            pak commands:
              csmanager pak list <file.pak>
              csmanager pak find <paks-dir|@icarus> <needle> [--path-only] [--grep] [--extracted dir]
              csmanager pak build <content-dir> -o <out.pak> [--mount ../../../Game/]
              csmanager pak build-mod <mod-dir> [-o <out.pak>] [--mount ...] [--prepare] [--ue-pack]
              csmanager pak patch <base.pak> <overlay-dir> -o <out.pak>
              csmanager pak extract <file.pak> <out-dir>
              csmanager pak search <file-or-dir> [--pattern text] [--ext .json] [--max 100]
              csmanager pak cat <file.pak> <entry-path> [-o out.json]
              csmanager pak grep <file-or-dir> <needle> [--max N]
              csmanager pak ue extract <pak> <out-dir> [--filter *Recipe*]
              csmanager pak ue pack <content-dir> -o <out.pak> [--mount ../../../Icarus/] [-compress]
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

        try
        {
            return sub switch
            {
                "list" => List(args),
                "find" => Find(args),
                "build" => Build(args),
                "build-mod" => BuildMod(args),
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

        var archive = PakArchiveReader.Open(args[1]);
        Console.WriteLine($"{archive.FilePath}");
        Console.WriteLine($"mount: {archive.MountPoint}");
        Console.WriteLine($"version: {archive.Footer.Version}");
        Console.WriteLine($"files: {archive.Entries.Count}");

        foreach (var entry in archive.Entries.Values.OrderBy(e => e.Path, StringComparer.OrdinalIgnoreCase))
        {
            var flags = entry.IsEncrypted ? " enc" : string.Empty;
            var comp = entry.IsCompressed ? " zlib" : string.Empty;
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

        var mount = GetArg(args, "--mount")
            ?? package.Manifest.Pak?.MountPoint
            ?? "../../../Icarus/Content/data/Crafting/";

        var useUePack = HasFlag(args, "--ue-pack")
            || package.Manifest.Pak?.UseUnrealPak == true
            || !string.IsNullOrWhiteSpace(package.Manifest.Pak?.SourcePak)
            || string.Equals(package.Manifest.Target?.GameId, "Icarus", StringComparison.OrdinalIgnoreCase);

        var hasCode = ModCodeCompiler.HasCodeProject(package);
        var shouldPrepare = HasFlag(args, "--prepare")
            || package.Manifest.PatchFiles.Count > 0
            || hasCode;

        string contentRoot;
        if (shouldPrepare && (package.Manifest.PatchFiles.Count > 0 || hasCode))
        {
            string? compiledAssembly = null;
            if (hasCode)
            {
                var compiled = ModCodeCompiler.Compile(package);
                compiledAssembly = compiled.AssemblyPath;
                Console.WriteLine($"compiled: {compiledAssembly}");
            }

            var sourcePak = package.Manifest.Pak?.SourcePak;
            if (sourcePak == "@icarus-data")
                sourcePak = cfg.ResolveIcarusDataPak();

            var prepared = ModAssetPreparer.Prepare(package, new ModPrepareOptions
            {
                SourcePakPath = sourcePak,
                ExtractedDir = cfg.ResolveDemoExtractedDir(),
                UnrealPakExecutable = cfg.UnrealPak,
                CompiledAssemblyPath = compiledAssembly,
                ForceExtract = HasFlag(args, "--force-extract"),
            });
            contentRoot = prepared.PreparedContentDir;
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
            var ue = new UnrealPakOptions
            {
                ExecutablePath = cfg.UnrealPak,
                EngineDir = cfg.UnrealEngineDir
                    ?? (string.IsNullOrWhiteSpace(cfg.IcarusPaksDir)
                        ? null
                        : Path.GetFullPath(Path.Combine(cfg.IcarusPaksDir, "..", "..", "..", "..", "Engine"))),
            };
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

        var archive = PakArchiveReader.Open(args[1]);
        PakEntryExtractor.ExtractToDirectory(archive, args[2]);
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
            : PakArchiveSearch.SearchFile(PakArchiveReader.Open(target), options);

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

        var archive = PakArchiveReader.Open(pakPath);
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
        var data = PakEntryExtractor.ReadEntry(stream, entry, archive.Footer);
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
        if (target == "@icarus" && !string.IsNullOrWhiteSpace(cfg.IcarusPaksDir))
            target = cfg.IcarusPaksDir;

        var needle = args[2];
        var max = 30;
        if (int.TryParse(GetArg(args, "--max"), out var parsedMax) && parsedMax > 0)
            max = parsedMax;

        var extracted = GetArg(args, "--extracted") ?? cfg.DemoExtractedDir;
        var hits = PakFind.Find(target, needle, new PakFindOptions
        {
            MaxResults = max,
            PathOnly = HasFlag(args, "--path-only"),
            GrepContent = HasFlag(args, "--grep"),
            ExtractedDir = extracted,
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
            Console.WriteLine("tip: game data often in .uasset (Oodle). Export JSON via FModel, or:");
            Console.WriteLine("  pak ue extract <pak> ./extracted -filter *Processor*");
            Console.WriteLine("  pak find ./extracted RequiredMillijoules");
        }

        return 0;
    }

    private static int UeExtract(string[] args)
    {
        if (args.Length < 3)
            return Missing("ue extract <pak> <out-dir> [--filter wildcard]");

        var cfg = StratwareConfig.Load();
        var pak = args[0];
        var outDir = args[1];
        var filter = GetArg(args, "--filter") ?? GetArg(args, "-f");
        var ue = new UnrealPakOptions { ExecutablePath = cfg.UnrealPak };
        UnrealPakRunner.Extract(pak, outDir, filter, ue);
        Console.WriteLine($"UnrealPak extract -> {Path.GetFullPath(outDir)}");
        return 0;
    }

    private static int UePack(string[] args)
    {
        if (args.Length < 2)
            return Missing("ue pack <content-dir> -o <out.pak> [--mount ../../../Icarus/]");

        var content = args[0];
        var output = GetArg(args, "-o") ?? GetArg(args, "--output");
        if (output is null)
            return Missing("ue pack <content-dir> -o <out.pak>");

        var cfg = StratwareConfig.Load();
        var mount = GetArg(args, "--mount") ?? cfg.IcarusMountPoint ?? "../../../Icarus/";
        var ue = new UnrealPakOptions { ExecutablePath = cfg.UnrealPak };
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
            : PakContentSearch.GrepFile(PakArchiveReader.Open(target), needle, max);

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
}
