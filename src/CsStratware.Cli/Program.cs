using CsStratware.Cli;
using CsStratware.Core.Models;
using CsStratware.ModLoader;

static void PrintUsage()
{
    Console.WriteLine("""
        csmanager — UE4 modding framework CLI

        Usage:
          csmanager list <mods-dir>              List discovered mods
          csmanager validate <mods-dir>          Validate mod manifests and layout
          csmanager compile <mod-dir> [--prepare]  Build mod C# patches (compile help)
          csmanager pak <subcommand> [args...]   UE4 .pak tools (pak help)
          csmanager setup <subcommand>           Install tools (setup help)
          csmanager help                         Show this help
        """);
}

if (args.Length == 0 || args[0] is "-h" or "--help" or "help")
{
    PrintUsage();
    return args.Length == 0 ? 1 : 0;
}

var command = args[0].ToLowerInvariant();

if (command == "pak")
    return PakCommands.Run(args[1..]);

if (command == "compile")
    return CompileCommands.Run(args[1..]);

if (command == "setup")
    return SetupCommands.Run(args[1..]);

var modsDir = args.Length > 1 ? args[1] : Path.Combine(Directory.GetCurrentDirectory(), "mods");

var loader = new JsonModLoader();
var result = await loader.LoadAsync(modsDir);

switch (command)
{
    case "list":
        foreach (var mod in result.Mods)
            Console.WriteLine($"{mod.Manifest.Id} {mod.Manifest.Version} — {mod.Manifest.Name}");
        break;

    case "validate":
        foreach (var issue in result.Issues)
        {
            var prefix = issue.Severity switch
            {
                ModIssueSeverity.Error => "ERROR",
                ModIssueSeverity.Warning => "WARN ",
                _ => "INFO ",
            };
            var mod = issue.ModId is null ? "" : $" [{issue.ModId}]";
            Console.WriteLine($"{prefix}{mod} {issue.Message}");
        }
        if (result.Mods.Count == 0)
            Console.WriteLine("No mods found.");
        else
            Console.WriteLine($"Validated {result.Mods.Count} mod(s). Success: {result.Success}");
        break;

    default:
        Console.Error.WriteLine($"Unknown command: {command}");
        PrintUsage();
        return 1;
}

return result.Success ? 0 : 2;
