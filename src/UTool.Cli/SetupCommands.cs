using UTool.Pak;

namespace UTool.Cli;

internal static class SetupCommands
{
    public static void PrintUsage()
    {
        Console.WriteLine("""
            setup commands:
              utool setup unrealpak [--from <Engine-dir>] [--force] [--appdata]

            Extracts bundled assets/UnrealPak.zip into assets/UnrealPak/ (utool repo),
            or copies an Engine tree with --from into tools/UnrealPak or %LocalAppData%/utool/UnrealPak/.
            """);
    }

    public static int Run(string[] args)
    {
        if (CliArgs.IsHelp(args))
        {
            PrintUsage();
            return 0;
        }

        var sub = args[0].ToLowerInvariant();
        if (sub != "unrealpak")
            return CliCommand.Unknown(sub, PrintUsage, "setup command");

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
        var cfg = UToolConfig.Load();
        var from = CliArgs.GetArg(args, "--from");
        var force = CliArgs.HasFlag(args, "--force");

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
                "No UnrealPak source. Clone utool with assets/UnrealPak.zip, or use " +
                "--from <path/to/Engine> / utool.json unrealEngineDir.");
            return 1;
        }

        var preferAppData = CliArgs.HasFlag(args, "--appdata");
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
