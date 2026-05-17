namespace CsStratware.Pak;

public sealed class UnrealPakToolchainPaths
{
    public required string Executable { get; init; }
    public required string EngineDir { get; init; }
    public required string StoreRoot { get; init; }
}

public static class UnrealPakToolchain
{
    public const string BundleFolderName = "UnrealPak";
    public const string RelativeExecutable = @"Engine\Binaries\Win64\UnrealPak.exe";

    public static string AppDataStoreRoot =>
        Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "csmanager",
            BundleFolderName);

    public static UnrealPakToolchainPaths Resolve(
        string? configExecutable = null,
        string? configEngineDir = null,
        string? configDirectory = null,
        bool ensureLocalCopy = true)
    {
        foreach (var store in EnumerateStoreRoots(configDirectory))
        {
            if (TryFromStore(store, out var local))
                return local;
        }

        var sourceEngine = InferSourceEngineDir(configExecutable, configEngineDir);
        if (ensureLocalCopy && sourceEngine is not null)
        {
            var targetStore = PickWritableStore(configDirectory);
            SyncFromSource(sourceEngine, targetStore, force: false);
            if (TryFromStore(targetStore, out var synced))
                return synced;
        }

        if (!string.IsNullOrWhiteSpace(configExecutable) && File.Exists(configExecutable))
        {
            var engine = configEngineDir;
            if (string.IsNullOrWhiteSpace(engine))
                engine = TryInferEngineDirFromExecutable(configExecutable);
            return new UnrealPakToolchainPaths
            {
                Executable = Path.GetFullPath(configExecutable),
                EngineDir = Path.GetFullPath(engine ?? Path.GetDirectoryName(configExecutable)!),
                StoreRoot = Path.GetDirectoryName(configExecutable)!,
            };
        }

        var fallbackExe = UnrealPakRunner.ResolveExecutable(new UnrealPakOptions
        {
            ExecutablePath = configExecutable,
            EngineDir = configEngineDir,
        });
        var fallbackEngine = configEngineDir;
        if (string.IsNullOrWhiteSpace(fallbackEngine))
            fallbackEngine = TryInferEngineDirFromExecutable(fallbackExe);
        return new UnrealPakToolchainPaths
        {
            Executable = fallbackExe,
            EngineDir = Path.GetFullPath(fallbackEngine ?? Path.GetDirectoryName(fallbackExe)!),
            StoreRoot = Path.GetDirectoryName(fallbackExe)!,
        };
    }

    public static UnrealPakToolchainPaths SyncFromSource(
        string sourceEngineDir,
        string? configDirectory = null,
        bool force = false,
        bool preferAppData = false)
    {
        sourceEngineDir = Path.GetFullPath(sourceEngineDir);
        ValidateSourceEngine(sourceEngineDir);

        var store = preferAppData
            ? AppDataStoreRoot
            : PickWritableStore(configDirectory);
        CopyEngineTree(sourceEngineDir, store, force);
        return TryFromStore(store, out var paths)
            ? paths
            : throw new InvalidOperationException($"UnrealPak sync failed under {store}");
    }

    public static UnrealPakOptions ToOptions(UnrealPakToolchainPaths paths) =>
        new()
        {
            ExecutablePath = paths.Executable,
            EngineDir = paths.EngineDir,
        };

    public static IEnumerable<string> EnumerateStoreRoots(string? configDirectory)
    {
        if (!string.IsNullOrWhiteSpace(configDirectory))
        {
            yield return Path.Combine(configDirectory, "tools", BundleFolderName);
            yield return Path.Combine(configDirectory, BundleFolderName);
        }

        var exeDir = GetProcessDirectory();
        if (!string.IsNullOrWhiteSpace(exeDir))
        {
            yield return Path.Combine(exeDir, "tools", BundleFolderName);
            yield return Path.Combine(exeDir, BundleFolderName);
        }

        yield return AppDataStoreRoot;
    }

    public static string? InferSourceEngineDir(string? configExecutable, string? configEngineDir)
    {
        if (!string.IsNullOrWhiteSpace(configEngineDir) && Directory.Exists(configEngineDir))
            return Path.GetFullPath(configEngineDir);

        if (!string.IsNullOrWhiteSpace(configExecutable) && File.Exists(configExecutable))
            return TryInferEngineDirFromExecutable(configExecutable);

        return null;
    }

    public static string PickWritableStore(string? configDirectory)
    {
        if (!string.IsNullOrWhiteSpace(configDirectory))
        {
            var projectTools = Path.Combine(configDirectory, "tools", BundleFolderName);
            if (TryEnsureWritableDirectory(projectTools))
                return projectTools;
        }

        if (!TryEnsureWritableDirectory(AppDataStoreRoot))
            throw new InvalidOperationException($"Cannot write UnrealPak store: {AppDataStoreRoot}");

        return AppDataStoreRoot;
    }

    public static bool TryFromStore(string storeRoot, out UnrealPakToolchainPaths paths)
    {
        paths = null!;
        var exe = Path.Combine(storeRoot, RelativeExecutable);
        if (!File.Exists(exe))
            return false;

        var engineDir = Path.Combine(storeRoot, "Engine");
        paths = new UnrealPakToolchainPaths
        {
            Executable = Path.GetFullPath(exe),
            EngineDir = Path.GetFullPath(engineDir),
            StoreRoot = Path.GetFullPath(storeRoot),
        };
        return true;
    }

    private static void ValidateSourceEngine(string sourceEngineDir)
    {
        var exe = Path.Combine(sourceEngineDir, "Binaries", "Win64", "UnrealPak.exe");
        if (!File.Exists(exe))
        {
            throw new FileNotFoundException(
                "Source engine folder must contain Binaries/Win64/UnrealPak.exe " +
                "(Icarus Mod Manager ships this under modmanager/UnrealPak/Engine).",
                exe);
        }
    }

    private static void CopyEngineTree(string sourceEngineDir, string destStoreRoot, bool force)
    {
        ValidateSourceEngine(sourceEngineDir);
        var destEngine = Path.Combine(destStoreRoot, "Engine");
        var destExe = Path.Combine(destEngine, "Binaries", "Win64", "UnrealPak.exe");
        if (!force && File.Exists(destExe))
            return;

        Directory.CreateDirectory(destStoreRoot);
        if (Directory.Exists(destEngine))
            Directory.Delete(destEngine, recursive: true);

        CopyDirectory(sourceEngineDir, destEngine);
    }

    private static void CopyDirectory(string sourceDir, string destDir)
    {
        Directory.CreateDirectory(destDir);
        foreach (var file in Directory.EnumerateFiles(sourceDir, "*", SearchOption.AllDirectories))
        {
            var relative = Path.GetRelativePath(sourceDir, file);
            var target = Path.Combine(destDir, relative);
            var targetParent = Path.GetDirectoryName(target);
            if (!string.IsNullOrEmpty(targetParent))
                Directory.CreateDirectory(targetParent);
            File.Copy(file, target, overwrite: true);
        }
    }

    private static string? TryInferEngineDirFromExecutable(string executablePath)
    {
        var dir = new FileInfo(Path.GetFullPath(executablePath)).Directory;
        for (var i = 0; i < 6 && dir is not null; i++)
        {
            if (string.Equals(dir.Name, "Engine", StringComparison.OrdinalIgnoreCase))
                return dir.FullName;
            dir = dir.Parent;
        }

        return null;
    }

    private static bool TryEnsureWritableDirectory(string path)
    {
        try
        {
            Directory.CreateDirectory(path);
            var probe = Path.Combine(path, ".write-test");
            File.WriteAllText(probe, "ok");
            File.Delete(probe);
            return true;
        }
        catch
        {
            return false;
        }
    }

    private static string? GetProcessDirectory()
    {
        var processPath = Environment.ProcessPath;
        return string.IsNullOrWhiteSpace(processPath)
            ? null
            : Path.GetDirectoryName(processPath);
    }
}
