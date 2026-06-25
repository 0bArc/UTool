using System.Text;
using UTool.Pak;

namespace UTool.Cli;

internal static partial class PakCommands
{
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

    private static int Patch(string[] args)
    {
        if (args.Length < 3)
            return Missing("patch <base.pak> <overlay-dir> -o <out.pak>");

        var output = CliArgs.GetArgAny(args, "-o", "--output");
        if (output is null)
            return Missing("patch <base.pak> <overlay-dir> -o <out.pak>");

        var mount = CliArgs.GetArg(args, "--mount");
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
        var pattern = CliArgs.GetArgAny(args, "--pattern", "-p");
        var extArg = CliArgs.GetArgAny(args, "--ext", "-e");
        var extensions = string.IsNullOrWhiteSpace(extArg)
            ? Array.Empty<string>()
            : extArg.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);
        var max = 500;
        if (CliArgs.TryGetPositiveInt(args, "--max", out var parsedMax))
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
        var output = CliArgs.GetArgAny(args, "-o", "--output");

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

        var cfg = UToolConfig.Load();
        var target = args[1];
        var gameId = CliArgs.GetArg(args, "--game");
        if (UToolConfig.IsPaksDirAlias(target))
        {
            var paks = cfg.ResolvePaksDir(gameId);
            if (string.IsNullOrWhiteSpace(paks))
            {
                Console.Error.WriteLine("gamePaksDir not set in utool.json (or games.<gameId>.paksDir).");
                return 1;
            }

            target = paks;
        }

        var needle = args[2];
        var max = 30;
        if (CliArgs.TryGetPositiveInt(args, "--max", out var parsedMax))
            max = parsedMax;

        var extracted = CliArgs.GetArg(args, "--extracted") ?? cfg.ResolveExtractedDir();
        var hits = PakFind.Find(target, needle, new PakFindOptions
        {
            MaxResults = max,
            PathOnly = CliArgs.HasFlag(args, "--path-only"),
            GrepContent = CliArgs.HasFlag(args, "--grep"),
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

    private static int Grep(string[] args)
    {
        if (args.Length < 3)
            return Missing("grep <file.pak|paks-dir> <needle> [--max N]");

        var target = args[1];
        var needle = args[2];
        var max = 50;
        if (CliArgs.TryGetPositiveInt(args, "--max", out var parsedMax))
            max = parsedMax;

        var matches = Directory.Exists(target)
            ? PakContentSearch.GrepDirectory(target, needle, max)
            : PakContentSearch.GrepFile(PakArchiveCache.Open(target, ResolvePakOpenOptions(args)), needle, max);

        foreach (var match in matches)
            Console.WriteLine($"{match.PakPath} :: {match.EntryPath} @0x{match.Offset:X} ({match.UncompressedSize} bytes)");

        Console.WriteLine($"matches: {matches.Count}");
        return 0;
    }
}
