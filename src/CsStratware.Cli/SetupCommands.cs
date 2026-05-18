using CsStratware.Pak;

namespace CsStratware.Cli;

internal static class SetupCommands
{
    public static void PrintUsage()
    {
        Console.WriteLine("""
            setup commands:
              csmanager setup unrealpak [--from <Engine-dir>] [--force] [--appdata]

            Extracts bundled assets/UnrealPak.zip into assets/UnrealPak/ (csStratware repo),
            or copies an Engine tree with --from into tools/UnrealPak or %LocalAppData%/csmanager/UnrealPak/.
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
        var force = HasFlag(args, "--force");

        if (string.IsNullOrWhiteSpace(from))
        {
            if (UnrealPakBundle.TryEnsureExtracted(cfg.ConfigDirectory, force))
            {
                var bundled = UnrealPakToolchain.Resolve(
                    cfg.UnrealPak,
                    cfg.UnrealEngineDir,
                    cfg.ConfigDirectory,
                    ensureLocalCopy: false);
                Console.WriteLine("UnrealPak ready (bundled assets):");
                Console.WriteLine($"  store:  {bundled.StoreRoot}");
                Console.WriteLine($"  engine: {bundled.EngineDir}");
                Console.WriteLine($"  exe:    {bundled.Executable}");
                return 0;
            }
        }

        var sourceEngine = ResolveSourceEngineArg(from)
            ?? UnrealPakToolchain.InferSourceEngineDir(cfg.UnrealPak, cfg.UnrealEngineDir, cfg.ConfigDirectory)
            ?? UnrealPakToolchain.TryDefaultEngineDir(cfg.ConfigDirectory);
        if (sourceEngine is null)
        {
            Console.Error.WriteLine(
                "No UnrealPak source. Clone csStratware with assets/UnrealPak.zip, or use " +
                "--from <path/to/Engine> / csstratware.json unrealEngineDir.");
            return 1;
        }

        var preferAppData = HasFlag(args, "--appdata");
        Console.WriteLine($"Source: {sourceEngine}");
        var paths = UnrealPakToolchain.SyncFromSource(
            sourceEngine,
            cfg.ConfigDirectory,
            force,
            preferAppData);
        Console.WriteLine("UnrealPak ready:");
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
