using System.Diagnostics;

var root = FindRepoRoot();
var sln = Path.Combine(root, "utool.sln");
var cli = Path.Combine(root, "src", "UTool.Cli", "UTool.Cli.csproj");
var dist = Path.Combine(root, "dist", "utool");
var configuration = ArgsValue("--configuration", "-c") ?? "Release";

Console.WriteLine($"repo: {root}");
Console.WriteLine($"config: {configuration}");

Run("dotnet", $"build \"{sln}\" -c {configuration} --nologo -v m");
Directory.CreateDirectory(dist);
Run("dotnet", $"publish \"{cli}\" -c {configuration} -o \"{dist}\" --nologo -v m");

var exe = Path.Combine(dist, OperatingSystem.IsWindows() ? "utool.exe" : "utool");
Console.WriteLine();
Console.WriteLine($"OK: {exe}");
Console.WriteLine($"PATH: add \"{dist}\"");

static string FindRepoRoot()
{
    foreach (var start in new[] { Directory.GetCurrentDirectory(), AppContext.BaseDirectory })
    {
        var dir = start;
        for (var i = 0; i < 12; i++)
        {
            if (File.Exists(Path.Combine(dir, "utool.sln")))
                return dir;
            var parent = Directory.GetParent(dir);
            if (parent is null)
                break;
            dir = parent.FullName;
        }
    }

    throw new InvalidOperationException("Could not find utool.sln (run from repo root).");
}

static string? ArgsValue(params string[] flags)
{
    var args = Environment.GetCommandLineArgs();
    for (var i = 1; i < args.Length - 1; i++)
    {
        if (flags.Any(f => string.Equals(args[i], f, StringComparison.OrdinalIgnoreCase)))
            return args[i + 1];
    }

    return null;
}

static void Run(string file, string arguments)
{
    Console.WriteLine($"> {file} {arguments}");
    using var process = Process.Start(new ProcessStartInfo
    {
        FileName = file,
        Arguments = arguments,
        WorkingDirectory = Directory.GetCurrentDirectory(),
        UseShellExecute = false,
    }) ?? throw new InvalidOperationException($"Failed to start {file}.");

    process.WaitForExit();
    if (process.ExitCode != 0)
        Environment.Exit(process.ExitCode);
}
