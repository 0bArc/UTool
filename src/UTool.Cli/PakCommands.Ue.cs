using UTool.Pak;

namespace UTool.Cli;

internal static partial class PakCommands
{
    private static int UeExtract(string[] args)
    {
        if (args.Length < 2)
            return Missing("ue extract <pak|paks-dir|@paks> <out-dir> [--filter wildcard] [--game <id>]");

        var cfg = UToolConfig.Load();
        var gameId = CliArgs.GetArg(args, "--game");
        var paks = ResolvePakSources(args[0], cfg, gameId);
        var outDir = args[1];
        var filter = CliArgs.GetArg(args, "--filter")
            ?? CliArgs.GetArg(args, "-filter")
            ?? CliArgs.GetArg(args, "-f");
        var pakOptions = ResolvePakOpenOptions(args, cfg, gameId);
        var ue = UnrealPakToolchain.ToOptions(cfg.ResolveUnrealPakToolchain(), pakOptions?.AesKey);

        Directory.CreateDirectory(outDir);
        foreach (var pak in paks)
        {
            var dest = paks.Count == 1
                ? outDir
                : Path.Combine(outDir, Path.GetFileNameWithoutExtension(pak));
            Directory.CreateDirectory(dest);
            UnrealPakRunner.Extract(pak, dest, filter, ue);
            Console.WriteLine($"UnrealPak extract: {pak} -> {Path.GetFullPath(dest)}");
        }

        Console.WriteLine($"UnrealPak extract done ({paks.Count} pak(s)) -> {Path.GetFullPath(outDir)}");
        return 0;
    }

    private static int UePack(string[] args)
    {
        if (args.Length < 2)
            return Missing("ue pack <content-dir> -o <out.pak> [--mount <ue-mount>] [--game <gameId>]");

        var content = args[0];
        var output = CliArgs.GetArgAny(args, "-o", "--output");
        if (output is null)
            return Missing("ue pack <content-dir> -o <out.pak>");

        var cfg = UToolConfig.Load();
        var mount = CliArgs.GetArg(args, "--mount") ?? cfg.ResolveMountPoint(CliArgs.GetArg(args, "--game"));
        if (string.IsNullOrWhiteSpace(mount))
        {
            Console.Error.WriteLine("Mount point required: --mount or defaultMountPoint in utool.json.");
            return 1;
        }

        var ue = UnrealPakToolchain.ToOptions(cfg.ResolveUnrealPakToolchain());
        UnrealPakRunner.PackDirectory(content, output, mount, CliArgs.HasFlag(args, "-compress"), ue);
        Console.WriteLine($"UnrealPak pack -> {Path.GetFullPath(output)}");
        return 0;
    }
}
