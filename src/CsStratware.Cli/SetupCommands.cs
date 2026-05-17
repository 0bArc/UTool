using CsStratware.Pak;

namespace CsStratware.Cli;

internal static class SetupCommands
{
    public static void PrintUsage()
    {
        Console.WriteLine("""
            setup commands:
              csmanager setup unrealpak [--from <Engine-dir>] [--force] [--appdata]

            Copies Icarus Mod Manager's UnrealPak Engine tree into a local store:
              <project>/tools/UnrealPak/   (when csstratware.json is in cwd tree)
              %LocalAppData%/csmanager/UnrealPak/   (fallback, or with --appdata)

            One-time: install Icarus Mod Manager, then point --from at its Engine folder
            or set unrealEngineDir / unrealPak in csstratware.json and run without --from.
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
        if (sub != "unrealpak")
        {
            Console.Error.WriteLine($"Unknown setup command: {sub}");
            PrintUsage();
            return 1;
        }

        try
        {
            return SetupUnrealPak(args[1..]);
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"setup error: {ex.Message}");
            return 1;
        }
    }

    private static int SetupUnrealPak(string[] args)
    {
        var cfg = StratwareConfig.Load();
        var from = GetArg(args, "--from");
        var sourceEngine = ResolveSourceEngineArg(from)
            ?? UnrealPakToolchain.InferSourceEngineDir(cfg.UnrealPak, cfg.UnrealEngineDir);
        if (sourceEngine is null)
        {
            Console.Error.WriteLine(
                "No source Engine folder. Use --from <path/to/Engine> or csstratware.json " +
                "unrealEngineDir / unrealPak (Icarus Mod Manager: .../modmanager/UnrealPak/Engine).");
            return 1;
        }

        var preferAppData = HasFlag(args, "--appdata");
        var force = HasFlag(args, "--force");
        Console.WriteLine($"Source: {sourceEngine}");
        var paths = UnrealPakToolchain.SyncFromSource(
            sourceEngine,
            cfg.ConfigDirectory,
            force,
            preferAppData);
        Console.WriteLine($"UnrealPak ready:");
        Console.WriteLine($"  store:  {paths.StoreRoot}");
        Console.WriteLine($"  engine: {paths.EngineDir}");
        Console.WriteLine($"  exe:    {paths.Executable}");
        return 0;
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

    private static string? ResolveSourceEngineArg(string? from)
    {
        if (string.IsNullOrWhiteSpace(from))
            return null;
        if (File.Exists(from))
            return UnrealPakToolchain.InferSourceEngineDir(from, null);
        if (Directory.Exists(from))
            return UnrealPakToolchain.InferSourceEngineDir(null, from);
        return null;
    }
}
