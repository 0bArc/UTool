using System.Diagnostics;
using System.Text;

namespace CsStratware.Pak;

public sealed class UnrealPakOptions
{
    public string? ExecutablePath { get; init; }
    public string? EngineDir { get; init; }
    public string? ProjectDir { get; init; }
    public string? EncryptionIni { get; init; }
}

public static class UnrealPakRunner
{
    public static string ResolveExecutable(UnrealPakOptions? options = null)
    {
        var fromOpt = options?.ExecutablePath;
        if (!string.IsNullOrWhiteSpace(fromOpt) && File.Exists(fromOpt))
            return Path.GetFullPath(fromOpt);

        var fromEnv = Environment.GetEnvironmentVariable("CSSTRATWARE_UNREALPAK");
        if (!string.IsNullOrWhiteSpace(fromEnv) && File.Exists(fromEnv))
            return Path.GetFullPath(fromEnv);

        var common = new[]
        {
            @"D:\SteamLibrary\steamapps\common\Icarus\modmanager\UnrealPak\Engine\Binaries\Win64\UnrealPak.exe",
            @"C:\software\UnrealPak.exe",
            @"C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealPak.exe",
            @"C:\Program Files\Epic Games\UE_5.3\Engine\Binaries\Win64\UnrealPak.exe",
            @"C:\Program Files\Epic Games\UE_5.2\Engine\Binaries\Win64\UnrealPak.exe",
            @"C:\Program Files\Epic Games\UE_5.1\Engine\Binaries\Win64\UnrealPak.exe",
            @"C:\Program Files\Epic Games\UE_5.0\Engine\Binaries\Win64\UnrealPak.exe",
            @"C:\Program Files\Epic Games\UE_4.27\Engine\Binaries\Win64\UnrealPak.exe",
        };

        foreach (var path in common)
        {
            if (File.Exists(path))
                return path;
        }

        throw new FileNotFoundException(
            "UnrealPak.exe not found. Set CSSTRATWARE_UNREALPAK or csstratware.json unrealPak path.");
    }

    public static void Extract(
        string pakPath,
        string outputDirectory,
        string? filter = null,
        UnrealPakOptions? options = null)
    {
        Directory.CreateDirectory(outputDirectory);
        var args = new List<string>
        {
            Quote(pakPath),
            "-Extract",
            Quote(outputDirectory),
        };
        if (!string.IsNullOrWhiteSpace(filter))
            args.Add($"-Filter={filter}");

        Run(args, options);
    }

    public static void Create(
        string outputPakPath,
        string responseFilePath,
        bool compress = false,
        UnrealPakOptions? options = null)
    {
        var args = new List<string>
        {
            Quote(outputPakPath),
            $"-Create={Quote(responseFilePath)}",
        };
        if (compress)
            args.Add("-compress");

        Run(args, options);
    }

    public static string WriteCreateResponseFile(
        string contentDirectory,
        string mountPoint,
        string responseFilePath)
    {
        mountPoint = mountPoint.Replace('\\', '/');
        if (!mountPoint.EndsWith('/'))
            mountPoint += '/';

        var lines = new List<string>();
        foreach (var file in Directory.EnumerateFiles(contentDirectory, "*", SearchOption.AllDirectories))
        {
            var relative = Path.GetRelativePath(contentDirectory, file).Replace('\\', '/');
            var pakPath = mountPoint + relative;
            lines.Add($"{Quote(file)} {Quote(pakPath)}");
        }

        if (lines.Count == 0)
            throw new InvalidOperationException($"No files under {contentDirectory}");

        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(responseFilePath))!);
        File.WriteAllLines(responseFilePath, lines, new UTF8Encoding(false));
        return responseFilePath;
    }

    public static void PackDirectory(
        string contentDirectory,
        string outputPakPath,
        string mountPoint,
        bool compress = false,
        UnrealPakOptions? options = null)
    {
        outputPakPath = Path.GetFullPath(outputPakPath);
        var response = Path.Combine(
            Path.GetTempPath(),
            "csstratware-" + Guid.NewGuid().ToString("N") + ".txt");
        try
        {
            WriteCreateResponseFile(contentDirectory, mountPoint, response);
            Create(outputPakPath, response, compress, options);
        }
        finally
        {
            try { File.Delete(response); } catch { /* ignore */ }
        }
    }

    private static void Run(IReadOnlyList<string> args, UnrealPakOptions? options)
    {
        var exe = ResolveExecutable(options);
        var allArgs = new List<string>(args);
        if (!string.IsNullOrWhiteSpace(options?.EngineDir))
            allArgs.Add($"-enginedir={Quote(options.EngineDir)}");
        if (!string.IsNullOrWhiteSpace(options?.ProjectDir))
            allArgs.Add($"-projectdir={Quote(options.ProjectDir)}");
        if (!string.IsNullOrWhiteSpace(options?.EncryptionIni))
            allArgs.Add($"-encryptionini={options.EncryptionIni}");

        var psi = new ProcessStartInfo
        {
            FileName = exe,
            Arguments = string.Join(' ', allArgs),
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
            CreateNoWindow = true,
        };

        using var process = Process.Start(psi)
            ?? throw new InvalidOperationException("Failed to start UnrealPak.");

        var stdout = process.StandardOutput.ReadToEnd();
        var stderr = process.StandardError.ReadToEnd();
        process.WaitForExit();

        if (process.ExitCode != 0)
        {
            throw new InvalidOperationException(
                $"UnrealPak failed (exit {process.ExitCode}).\n{stderr}\n{stdout}");
        }
    }

    private static string Quote(string path) =>
        path.Contains(' ') ? $"\"{path}\"" : path;
}
