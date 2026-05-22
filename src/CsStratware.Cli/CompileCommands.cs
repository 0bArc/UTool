using CsStratware.ModLoader;
using CsStratware.Pak;

namespace CsStratware.Cli;

internal static class CompileCommands
{
    public static void PrintUsage()
    {
        Console.WriteLine("""
            compile commands:
              csmanager compile <mod-dir>              Build mod code/*.csproj -> .cache/compiled
              csmanager compile <mod-dir> --prepare    Compile + stage patched JSON in .cache/prepared
            """);
    }

    public static int Run(string[] args)
    {
        if (args.Length == 0 || args[0] is "-h" or "--help" or "help")
        {
            PrintUsage();
            return 0;
        }

        var modDir = Path.GetFullPath(args[0]);
        var package = ModDiscovery.TryLoadPackageAsync(modDir).GetAwaiter().GetResult();
        if (package is null)
        {
            Console.Error.WriteLine($"Failed to load mod from {modDir}");
            return 1;
        }

        if (!ModCodeCompiler.HasCodeProject(package))
        {
            Console.Error.WriteLine($"No code project. Add {ModCodeCompiler.DefaultCodeDirName}/*.csproj or mod.json codeProject.");
            return 1;
        }

        try
        {
            var configuration = GetArg(args, "-c") ?? GetArg(args, "--configuration") ?? "Release";
            var result = ModCodeCompiler.Compile(package, configuration);
            Console.WriteLine($"compiled: {result.AssemblyPath}");

            var bundle = ModCodePatchRunner.LoadFromAssembly(result.AssemblyPath);
            foreach (var patch in bundle.AssetPatches)
                Console.WriteLine($"  asset: {patch.AssetFileName} ({patch.PatchType.Name})");
            foreach (var patch in bundle.PlayerDataPatches)
                Console.WriteLine($"  playerdata: {patch.RelativePath} ({patch.PatchType.Name})");

            if (!HasFlag(args, "--prepare"))
                return 0;

            var cfg = StratwareConfig.Load(modDir);
            var gameId = package.Manifest.Target?.GameId;
            var sourcePakToken = package.Manifest.Pak?.SourcePak;
            if (string.IsNullOrWhiteSpace(sourcePakToken)
                && (ModCodeCompiler.HasCodeProject(package) || package.Manifest.PatchFiles.Count > 0))
                sourcePakToken = "@data";
            var sourcePak = string.IsNullOrWhiteSpace(sourcePakToken)
                ? null
                : cfg.ResolveSourcePak(sourcePakToken, gameId);
            var curveSource = package.Manifest.Pak?.CurveSourcePak ?? "@paks";
            var curveSourcePaks = cfg.ResolveSourcePakPaths(curveSource, gameId);

            var toolchain = cfg.ResolveUnrealPakToolchain();
            var prepared = ModAssetPreparer.Prepare(package, new ModPrepareOptions
            {
                SourcePakPath = sourcePak,
                CurveSourcePakPaths = curveSourcePaks,
                PakAesKey = cfg.ResolvePakAesKey(gameId),
                UnrealPakOptions = UnrealPakToolchain.ToOptions(toolchain, cfg.ResolvePakAesKey(gameId)),
                ExtractedDir = cfg.ResolveExistingExtractedDir(),
                UnrealPakExecutable = toolchain.Executable,
                CompiledAssemblyPath = result.AssemblyPath,
                PlayerDataRoot = cfg.ResolvePlayerDataDir(gameId),
                ForceExtract = HasFlag(args, "--force-extract"),
            });

            foreach (var file in prepared.PreparedFiles)
                Console.WriteLine($"prepared: {file} ({new FileInfo(file).Length} bytes)");

            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"compile error: {ex.Message}");
            return 1;
        }
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
