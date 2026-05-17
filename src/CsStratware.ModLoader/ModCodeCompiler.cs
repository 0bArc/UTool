using System.Diagnostics;
using CsStratware.Core.Models;

namespace CsStratware.ModLoader;

public sealed class ModCompileResult
{
    public required string AssemblyPath { get; init; }
    public required string OutputDir { get; init; }
}

public static class ModCodeCompiler
{
    public const string DefaultCodeDirName = "code";
    public const string CompiledCacheDirName = "compiled";

    public static bool HasCodeProject(ModPackage mod) => TryResolveProjectPath(mod) is not null;

    public static string? TryResolveProjectPath(ModPackage mod)
    {
        if (!string.IsNullOrWhiteSpace(mod.Manifest.CodeProject))
        {
            var explicitPath = Path.Combine(mod.RootPath, mod.Manifest.CodeProject);
            return File.Exists(explicitPath) ? Path.GetFullPath(explicitPath) : null;
        }

        var codeDir = Path.Combine(mod.RootPath, DefaultCodeDirName);
        if (!Directory.Exists(codeDir))
            return null;

        var projects = Directory.GetFiles(codeDir, "*.csproj", SearchOption.TopDirectoryOnly);
        return projects.Length switch
        {
            0 => null,
            1 => Path.GetFullPath(projects[0]),
            _ => throw new InvalidOperationException(
                $"Multiple .csproj files in {codeDir}. Set mod.json codeProject to the one to build."),
        };
    }

    public static string ResolveCompiledDir(ModPackage mod) =>
        Path.Combine(mod.RootPath, ".cache", CompiledCacheDirName);

    public static ModCompileResult Compile(ModPackage mod, string configuration = "Release")
    {
        var csproj = TryResolveProjectPath(mod)
            ?? throw new InvalidOperationException(
                $"No code project for mod '{mod.Manifest.Id}'. Add code/*.csproj or mod.json codeProject.");

        var outDir = ResolveCompiledDir(mod);
        if (Directory.Exists(outDir))
        {
            try { Directory.Delete(outDir, recursive: true); } catch { /* ignore */ }
        }

        Directory.CreateDirectory(outDir);

        var args =
            $"build \"{csproj}\" -c {configuration} -o \"{outDir}\" --nologo -v:q /p:UseAppHost=false";
        var exit = RunProcess("dotnet", args, Path.GetDirectoryName(csproj)!);
        if (exit != 0)
            throw new InvalidOperationException($"dotnet build failed (exit {exit}) for {csproj}");

        var assemblyName = Path.GetFileNameWithoutExtension(csproj);
        var dll = Path.Combine(outDir, $"{assemblyName}.dll");
        if (!File.Exists(dll))
        {
            dll = Directory.GetFiles(outDir, "*.dll", SearchOption.TopDirectoryOnly)
                .FirstOrDefault(f => !Path.GetFileName(f).StartsWith("CsStratware.", StringComparison.OrdinalIgnoreCase))
                ?? throw new InvalidOperationException($"No mod assembly produced in {outDir}");
        }

        return new ModCompileResult
        {
            AssemblyPath = Path.GetFullPath(dll),
            OutputDir = Path.GetFullPath(outDir),
        };
    }

    private static int RunProcess(string fileName, string arguments, string workingDirectory)
    {
        using var process = Process.Start(new ProcessStartInfo
        {
            FileName = fileName,
            Arguments = arguments,
            WorkingDirectory = workingDirectory,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
        }) ?? throw new InvalidOperationException($"Failed to start {fileName}");

        var stdout = process.StandardOutput.ReadToEnd();
        var stderr = process.StandardError.ReadToEnd();
        process.WaitForExit();

        if (process.ExitCode != 0)
        {
            if (!string.IsNullOrWhiteSpace(stdout))
                Console.Error.WriteLine(stdout.TrimEnd());
            if (!string.IsNullOrWhiteSpace(stderr))
                Console.Error.WriteLine(stderr.TrimEnd());
        }

        return process.ExitCode;
    }
}
