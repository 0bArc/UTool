using System.Reflection;
using UTool.Infrastructure.Logging;
using UTool.Infrastructure.Operations;
using UTool.Pak;

namespace UTool.Cli;

internal static partial class PakCommands
{
    private static int Unknown(string sub) =>
        CliCommand.Unknown(sub, PrintUsage, "pak command");

    private static int Missing(string usage) =>
        CliCommand.Missing(usage, PrintUsage);

    private static PakOpenOptions? ResolvePakOpenOptions(
        string[] args,
        UToolConfig? cfg = null,
        string? gameId = null)
    {
        cfg ??= UToolConfig.Load();
        gameId ??= CliArgs.GetArg(args, "--game");
        var bytes = PakOpenOptions.ParseAesKey(CliArgs.GetArg(args, "--aes-key"))
            ?? PakOpenOptions.ParseAesKey(Environment.GetEnvironmentVariable("PAK_AES_KEY"))
            ?? cfg.ResolvePakAesKey(gameId);
        return bytes is null ? null : new PakOpenOptions { AesKey = bytes };
    }

    private static OperationContext CreateOperationContext(string[] args)
    {
        if (CliArgs.IsVerbose(args))
            UToolLog.MinimumLevel = LogLevel.Debug;

        var progress = CliArgs.HasFlag(args, "--progress")
            ? new Progress<OperationProgress>(p =>
            {
                var total = p.Total is int t ? $"/{t}" : "";
                Console.Error.WriteLine($"[{p.Current}{total}] {p.Message}");
            })
            : null;

        return new OperationContext { Progress = progress };
    }

    private static void EnsureHostSupportsCurvePatches()
    {
        if (Type.GetType("UTool.Sdk.CurvePatch, UTool.Sdk") is not null)
            return;

        var host = Assembly.GetExecutingAssembly().GetName();
        throw new InvalidOperationException(
            $"utool {host.Version} is missing CurvePatch API. Rebuild the CLI from csStratware " +
            "(run: dotnet run build.cs) and ensure PATH points at dist/utool, not an older install.");
    }

    private static IReadOnlyList<string> ResolvePakSources(string target, UToolConfig cfg, string? gameId)
    {
        if (UToolConfig.IsPaksDirAlias(target))
        {
            var paksDir = cfg.ResolvePaksDir(gameId);
            if (string.IsNullOrWhiteSpace(paksDir))
            {
                throw new InvalidOperationException(
                    "gamePaksDir not set in utool.json (or games.<gameId>.paksDir). Use --game or set gamePaksDir.");
            }

            return PakPathResolver.Resolve(paksDir);
        }

        if (UToolConfig.IsDataPakAlias(target))
        {
            var dataPak = cfg.ResolveDataPak(gameId);
            return PakPathResolver.Resolve(dataPak);
        }

        return PakPathResolver.Resolve(target);
    }

    private static IReadOnlyList<string> ResolveMergePakPaths(
        string target,
        UToolConfig cfg,
        string? gameId,
        string? excludeOutputPak = null)
    {
        if (UToolConfig.IsPaksDirAlias(target) || UToolConfig.IsDataPakAlias(target))
            return PakPathResolver.FilterMergeInputs(ResolvePakSources(target, cfg, gameId), excludeOutputPak);

        return PakPathResolver.ResolveForMerge(target, excludeOutputPak);
    }

    private static IReadOnlyList<string> ResolveCheckPakPaths(string target, UToolConfig cfg, string? gameId)
    {
        if (Directory.Exists(target))
            return ResolveMergePakPaths(target, cfg, gameId);

        return ResolvePakSources(target, cfg, gameId);
    }
}
