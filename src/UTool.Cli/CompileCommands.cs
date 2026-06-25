using UTool.ModLoader;
using UTool.Pak;

namespace UTool.Cli;

internal static class CompileCommands
{
    public static void PrintUsage()
    {
        Console.WriteLine("""
            compile commands:
              utool compile <mod-dir>              Build mod code/*.csproj -> .cache/compiled
              utool compile <mod-dir> --prepare    Compile + stage patched JSON in .cache/prepared
            """);
    }

    public static int Run(string[] args)
    {
        if (CliArgs.IsHelp(args))
        {
            PrintUsage();
            return 0;
        }

        var modDir = Path.GetFullPath(args[0]);
        var package = ModPackageCli.TryLoad(modDir);
        if (package is null)
        {
            Console.Error.WriteLine($"Failed to load mod from {modDir}");
            return 1;
        }

        ModCodeProjectScaffold.EnsureProject(package);
        if (!ModCodeCompiler.HasCodeProject(package))
        {
            Console.Error.WriteLine($"No code project. Add {ModCodeCompiler.DefaultCodeDirName}/*.csproj, *.cs, or mod.json codeProject.");
            return 1;
        }

        try
        {
            var configuration = CliArgs.GetArgAny(args, "-c", "--configuration") ?? "Release";
            var result = ModCodeCompiler.Compile(package, configuration);
            Console.WriteLine($"compiled: {result.AssemblyPath}");

            var bundle = ModCodePatchRunner.LoadFromAssembly(result.AssemblyPath);
            foreach (var patch in bundle.AssetPatches)
                Console.WriteLine($"  asset: {patch.AssetFileName} ({patch.PatchType.Name})");
            foreach (var patch in bundle.CurvePatches)
                Console.WriteLine($"  curve: {patch.AssetName} ({patch.Instance.GetType().Name})");
            foreach (var patch in bundle.PlayerDataPatches)
                Console.WriteLine($"  playerdata: {patch.RelativePath} ({patch.PatchType.Name})");

            if (!CliArgs.HasFlag(args, "--prepare"))
                return 0;

            var cfg = UToolConfig.Load(modDir);
            var prepared = ModPreparePipeline.Prepare(package, cfg, new ModPrepareCliOptions
            {
                CompiledAssemblyPath = result.AssemblyPath,
                ForceExtract = CliArgs.HasFlag(args, "--force-extract"),
            });
            ModPreparePipeline.WritePreparedFiles(prepared.PreparedFiles);

            return 0;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"compile error: {ex.Message}");
            return 1;
        }
    }
}
