using UTool.Core.Models;
using UTool.Infrastructure.Operations;
using UTool.ModLoader;
using UTool.Pak;

namespace UTool.Cli;

internal enum ModPrepareScope
{
    Minimal,
    Full,
}

internal sealed class ModPrepareCliOptions
{
    public ModPrepareScope Scope { get; init; } = ModPrepareScope.Full;
    public string? GameId { get; init; }
    public string? CompiledAssemblyPath { get; init; }
    public string? PlayerDataRoot { get; init; }
    public bool PreserveSourcePaths { get; init; }
    public bool ForceExtract { get; init; }
    public bool SkipIfUpToDate { get; init; } = true;
    public OperationContext? Operation { get; init; }
}

internal static class ModPreparePipeline
{
    public static ModPrepareResult Prepare(ModPackage package, UToolConfig cfg, ModPrepareCliOptions cli)
    {
        var gameId = cli.GameId ?? package.Manifest.Target?.GameId;
        var options = BuildOptions(package, cfg, gameId, cli);
        return ModAssetPreparer.Prepare(package, options);
    }

    public static ModPrepareOptions BuildOptions(
        ModPackage package,
        UToolConfig cfg,
        string? gameId,
        ModPrepareCliOptions cli)
    {
        var toolchain = cfg.ResolveUnrealPakToolchain();
        if (cli.Scope == ModPrepareScope.Minimal)
        {
            return new ModPrepareOptions
            {
                SourcePakPath = cfg.ResolveSourcePak(package.Manifest.Pak?.SourcePak, gameId),
                ExtractedDir = cfg.ResolveExistingExtractedDir(),
                UnrealPakExecutable = toolchain.Executable,
                CompiledAssemblyPath = cli.CompiledAssemblyPath,
                PlayerDataRoot = cli.PlayerDataRoot ?? cfg.ResolvePlayerDataDir(gameId),
                PreserveSourcePaths = cli.PreserveSourcePaths,
                ForceExtract = cli.ForceExtract,
                Operation = cli.Operation,
                SkipIfUpToDate = cli.SkipIfUpToDate,
            };
        }

        var sourcePakToken = ResolveSourcePakToken(package);
        var sourcePak = string.IsNullOrWhiteSpace(sourcePakToken)
            ? null
            : cfg.ResolveSourcePak(sourcePakToken, gameId);
        var curveSource = package.Manifest.Pak?.CurveSourcePak ?? "@paks";
        var curveSourcePaks = cfg.ResolveSourcePakPaths(curveSource, gameId);
        var aesKey = cfg.ResolvePakAesKey(gameId);

        return new ModPrepareOptions
        {
            SourcePakPath = sourcePak,
            CurveSourcePakPaths = curveSourcePaks,
            PakAesKey = aesKey,
            UnrealPakOptions = UnrealPakToolchain.ToOptions(toolchain, aesKey),
            ExtractedDir = cfg.ResolveExistingExtractedDir(),
            UnrealPakExecutable = toolchain.Executable,
            CompiledAssemblyPath = cli.CompiledAssemblyPath,
            PlayerDataRoot = cli.PlayerDataRoot ?? cfg.ResolvePlayerDataDir(gameId),
            PreserveSourcePaths = cli.PreserveSourcePaths,
            ForceExtract = cli.ForceExtract,
            Operation = cli.Operation,
            SkipIfUpToDate = cli.SkipIfUpToDate,
        };
    }

    public static void WritePreparedFiles(IEnumerable<string> files)
    {
        foreach (var file in files)
            Console.WriteLine($"prepared: {file} ({new FileInfo(file).Length} bytes)");
    }

    private static string? ResolveSourcePakToken(ModPackage package)
    {
        var token = package.Manifest.Pak?.SourcePak;
        if (string.IsNullOrWhiteSpace(token)
            && (ModCodeCompiler.HasCodeProject(package) || package.Manifest.PatchFiles.Count > 0))
            token = "@data";

        return string.IsNullOrWhiteSpace(token) ? null : token;
    }
}
