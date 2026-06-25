using UTool.Core.Models;
using UTool.Infrastructure.PlayerData;
using UTool.ModLoader;
using UTool.Pak;
using UTool.Sdk;

namespace UTool.Cli;

internal static class PlayerDataCommands
{
    public static void PrintUsage()
    {
        Console.WriteLine("""
            playerdata commands:
              utool playerdata list [--root <dir>] [--game <gameId>]
              utool playerdata status <mod-dir> [--root <dir>] [--game <gameId>]
              utool playerdata prepare <mod-dir> [--root <dir>] [--game <gameId>] [--force-extract]
              utool playerdata apply <mod-dir> [--profile <steamId>] [--dry-run] [--root <dir>] [--game <gameId>]
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
        var rest = args[1..];

        return sub switch
        {
            "list" => List(rest),
            "status" => Status(rest),
            "prepare" => Prepare(rest),
            "apply" => Apply(rest),
            _ => CliCommand.Unknown(sub, PrintUsage, "playerdata command"),
        };
    }

    private static int List(string[] args)
    {
        var cfg = UToolConfig.Load();
        var root = ResolveRoot(args, cfg, null, CliArgs.GetArg(args, "--game"));

        var store = new PlayerDataStore(root);
        if (!store.Exists)
        {
            Console.Error.WriteLine($"PlayerData not found: {root}");
            return 1;
        }

        foreach (var id in store.ListProfileIds())
        {
            Console.WriteLine(id);
            foreach (var file in store.EnumerateJsonFiles(id))
                Console.WriteLine($"  {file.RelativePath}");
        }

        return 0;
    }

    private static int Status(string[] args)
    {
        if (args.Length == 0 || args[0] is "-h" or "--help")
            return CliCommand.Missing("status <mod-dir>", PrintUsage, "Missing");

        var modDir = Path.GetFullPath(args[0]);
        var cfg = UToolConfig.Load(modDir);
        var package = ModPackageCli.TryLoad(modDir);
        if (package is null)
        {
            Console.Error.WriteLine($"Failed to load mod from {modDir}");
            return 1;
        }

        var gameId = ResolveGameId(args, package);
        var root = ResolveRoot(args, cfg, package, gameId);
        var saves = PlayerSaveReader.TryLoad(root);
        if (saves is null)
        {
            Console.Error.WriteLine($"PlayerData not found: {root}");
            return 1;
        }

        Console.WriteLine($"PlayerData: {root}");
        Console.WriteLine($"GameId: {gameId ?? "(none)"}");
        Console.WriteLine($"Profiles: {saves.ProfileIds.Count}");

        if (!ModCodeCompiler.HasCodeProject(package))
            return 0;

        var dll = ModCodeCompiler.Compile(package).AssemblyPath;
        var bundle = ModCodePatchRunner.LoadFromAssembly(dll, package.Manifest.Id);
        foreach (var patch in bundle.AssetPatches)
        {
            var active = patch.Instance is not ConditionalAssetPatch c || c.ShouldApply(saves);
            Console.WriteLine($"  asset {patch.AssetFileName} ({patch.PatchType.Name}): {(active ? "apply" : "skip")}");
        }

        foreach (var patch in bundle.PlayerDataPatches)
            Console.WriteLine($"  playerdata {patch.RelativePath} ({patch.PatchType.Name})");

        return 0;
    }

    private static int Prepare(string[] args)
    {
        if (args.Length == 0 || args[0] is "-h" or "--help")
            return CliCommand.Missing("prepare <mod-dir>", PrintUsage, "Missing");

        var modDir = Path.GetFullPath(args[0]);
        var cfg = UToolConfig.Load(modDir);
        var package = ModPackageCli.TryLoad(modDir);
        if (package is null)
        {
            Console.Error.WriteLine($"Failed to load mod from {modDir}");
            return 1;
        }

        if (!ModCodeCompiler.HasCodeProject(package))
        {
            Console.Error.WriteLine("Mod has no code project.");
            return 1;
        }

        var gameId = ResolveGameId(args, package);
        var compiled = ModCodeCompiler.Compile(package);
        var prepared = ModPreparePipeline.Prepare(package, cfg, new ModPrepareCliOptions
        {
            Scope = ModPrepareScope.Minimal,
            GameId = gameId,
            CompiledAssemblyPath = compiled.AssemblyPath,
            PlayerDataRoot = ResolveRoot(args, cfg, package, gameId),
            ForceExtract = CliArgs.HasFlag(args, "--force-extract"),
        });
        ModPreparePipeline.WritePreparedFiles(prepared.PreparedFiles);

        return 0;
    }

    private static int Apply(string[] args)
    {
        if (args.Length == 0 || args[0] is "-h" or "--help")
            return CliCommand.Missing("apply <mod-dir>", PrintUsage, "Missing");

        var modDir = Path.GetFullPath(args[0]);
        var cfg = UToolConfig.Load(modDir);
        var package = ModPackageCli.TryLoad(modDir);
        if (package is null)
        {
            Console.Error.WriteLine($"Failed to load mod from {modDir}");
            return 1;
        }

        if (!ModCodeCompiler.HasCodeProject(package))
        {
            Console.Error.WriteLine("Mod has no code project.");
            return 1;
        }

        var gameId = ResolveGameId(args, package);
        var root = ResolveRoot(args, cfg, package, gameId);
        var store = new PlayerDataStore(root);
        if (!store.Exists)
        {
            Console.Error.WriteLine($"PlayerData not found: {root}");
            return 1;
        }

        var dll = ModCodeCompiler.Compile(package).AssemblyPath;
        var bundle = ModCodePatchRunner.LoadFromAssembly(dll, package.Manifest.Id);
        if (bundle.PlayerDataPatches.Count == 0)
        {
            Console.Error.WriteLine("No [PatchPlayerData] types in mod assembly. Use playerdata prepare for [PatchAsset] mods.");
            return 1;
        }

        var profile = CliArgs.GetArg(args, "--profile");
        var dryRun = CliArgs.HasFlag(args, "--dry-run");
        var results = ModPlayerDataPatchRunner.ApplyAll(store, bundle.PlayerDataPatches, profile, dryRun);

        foreach (var r in results)
            Console.WriteLine($"{(r.Changed ? "changed" : "unchanged")}: {r.ProfileId}/{r.RelativePath}");

        return 0;
    }

    private static string? ResolveGameId(string[] args, ModPackage package) =>
        CliArgs.GetArg(args, "--game") ?? package.Manifest.Target?.GameId;

    private static string ResolveRoot(string[] args, UToolConfig? cfg, ModPackage? package, string? gameId = null)
    {
        gameId ??= package is null ? CliArgs.GetArg(args, "--game") : ResolveGameId(args, package);
        var fromArg = CliArgs.GetArg(args, "--root");
        if (!string.IsNullOrWhiteSpace(fromArg))
            return Path.GetFullPath(fromArg);

        if (cfg is not null)
            return cfg.ResolvePlayerDataDir(gameId);

        if (!string.IsNullOrWhiteSpace(gameId))
            return Ue4PlayerDataLocator.Resolve(gameId: gameId);

        throw new InvalidOperationException("Pass --root, --game, or set playerDataDir in utool.json.");
    }
}
