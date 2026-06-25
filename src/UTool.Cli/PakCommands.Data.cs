using UTool.Pak;

namespace UTool.Cli;

internal static partial class PakCommands
{
    private static int DataList(string[] args)
    {
        if (args.Length < 1)
            return Missing("data list <pak|paks-dir|@paks> [--pattern text] [--ext .json] [--game <id>]");

        var cfg = UToolConfig.Load();
        var gameId = CliArgs.GetArg(args, "--game");
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

        var cfg = UToolConfig.Load();
        var gameId = CliArgs.GetArg(args, "--game");
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
            UnrealPakFallback = !CliArgs.HasFlag(args, "--no-ue-fallback"),
            Log = CliArgs.IsVerbose(args) ? Console.WriteLine : null,
        };

        var result = PakDataPuller.Pull(paks, outDir, options);
        Console.WriteLine($"data pull -> {result.OutputDirectory}");
        Console.WriteLine($"written: {result.Written} (unrealpak: {result.UnrealPakExtracted}, deferred: {result.SkippedEncrypted})");
        if (result.Written == 0 && aesKey is null)
            Console.WriteLine("hint: encrypted paks need pakAesKey in utool.json, --aes-key, or PAK_AES_KEY.");
        return 0;
    }

    private static PakDataPullOptions BuildDataPullOptions(
        string[] args,
        UToolConfig? cfg = null,
        string? gameId = null)
    {
        cfg ??= UToolConfig.Load();
        var extArg = CliArgs.GetArgAny(args, "--ext", "-e");
        var extensions = string.IsNullOrWhiteSpace(extArg)
            ? new PakDataPullOptions().Extensions
            : extArg.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries);

        var max = 0;
        if (CliArgs.TryGetPositiveInt(args, "--max", out var parsedMax))
            max = parsedMax;

        return new PakDataPullOptions
        {
            Pattern = CliArgs.GetArgAny(args, "--pattern", "-p"),
            Extensions = extensions,
            MaxFiles = max,
            PakOpenOptions = ResolvePakOpenOptions(args, cfg, gameId),
        };
    }
}
