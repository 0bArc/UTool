namespace UTool.Cli;

internal static partial class PakCommands
{
    public static void PrintUsage()
    {
        Console.WriteLine("""
            pak commands:
              utool pak list <file.pak>
              utool pak find <paks-dir|@paks> <needle> [--game <gameId>] [--path-only] [--grep] [--extracted dir] [--aes-key hex]
              utool pak build <content-dir> -o <out.pak> [--mount ../../../Game/]
              utool pak build-mod <mod-dir> [-o <out.pak>] [--mount ...] [--prepare] [--ue-pack]
              utool pak check <mods-dir|paks-dir|@paks> [--game <id>] [--aes-key hex]
              utool pak merge <pak1> <pak2> [...] -o <out.pak> [--last-wins] [--mount ...] [--game <id>]
              utool pak merge-build <pak1> <pak2> [...] -o <out.pak> [--mods-dir dir] [--report dir] [--extract-dir path] [--game <id>] [-v|--verbose]
              utool pak patch <base.pak> <overlay-dir> -o <out.pak>
              utool pak extract <file.pak> <out-dir>
              utool pak search <file-or-dir> [--pattern text] [--ext .json] [--max 100]
              utool pak cat <file.pak> <entry-path> [-o out.json]
              utool pak grep <file-or-dir> <needle> [--max N]
              utool pak data list <pak|paks-dir|@paks> [--pattern *Recipe*] [--ext .json,.ini] [--game <id>] [--aes-key hex|base64]
              utool pak data pull <pak|paks-dir|@paks> <out-dir> [--pattern ...] [--ext ...] [--game <id>] [--aes-key ...] [--no-ue-fallback]
              utool pak ue extract <pak|paks-dir|@paks> <out-dir> [--filter *Recipe*] [--game <id>] [--aes-key ...]
              utool pak ue pack <content-dir> -o <out.pak> [--mount <ue-mount>] [--game <gameId>] [-compress]
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
        if (sub == "ue" && args.Length > 1)
        {
            var ueSub = args[1].ToLowerInvariant();
            var ueArgs = args[2..];
            try
            {
                return ueSub switch
                {
                    "extract" => UeExtract(ueArgs),
                    "pack" => UePack(ueArgs),
                    _ => Unknown($"ue {ueSub}"),
                };
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"pak error: {ex.Message}");
                return 1;
            }
        }

        if (sub == "data" && args.Length > 1)
        {
            var dataSub = args[1].ToLowerInvariant();
            var dataArgs = args[2..];
            try
            {
                return dataSub switch
                {
                    "list" => DataList(dataArgs),
                    "pull" => DataPull(dataArgs),
                    _ => Unknown($"data {dataSub}"),
                };
            }
            catch (Exception ex)
            {
                Console.Error.WriteLine($"pak error: {ex.Message}");
                return 1;
            }
        }

        try
        {
            return sub switch
            {
                "list" => List(args),
                "find" => Find(args),
                "build" => Build(args),
                "build-mod" => BuildMod(args),
                "check" => Check(args[1..]),
                "merge" => Merge(args[1..]),
                "merge-build" => MergeBuild(args[1..]),
                "patch" => Patch(args),
                "extract" => Extract(args),
                "search" => Search(args),
                "cat" => Cat(args),
                "grep" => Grep(args),
                _ => Unknown(sub),
            };
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine($"pak error: {ex.Message}");
            return 1;
        }
    }
}
