namespace CsStratware.Pak;

/// <summary>Remove dotnet and prepare staging dirs after a successful mod pak build.</summary>
public static class ModBuildCleanup
{
    public static void AfterPack(string modDir, bool keepCache = false)
    {
        modDir = Path.GetFullPath(modDir);
        RemoveDirIfExists(Path.Combine(modDir, "bin"));
        RemoveDirIfExists(Path.Combine(modDir, "obj"));

        var codeDir = Path.Combine(modDir, "code");
        if (Directory.Exists(codeDir))
        {
            RemoveDirIfExists(Path.Combine(codeDir, "bin"));
            RemoveDirIfExists(Path.Combine(codeDir, "obj"));
        }

        if (keepCache)
            return;

        GC.Collect();
        GC.WaitForPendingFinalizers();
        RemoveDirIfExists(Path.Combine(modDir, ".cache"));
    }

    private static void RemoveDirIfExists(string path)
    {
        if (!Directory.Exists(path))
            return;

        for (var attempt = 0; attempt < 5; attempt++)
        {
            try
            {
                Directory.Delete(path, recursive: true);
                if (!Directory.Exists(path))
                    return;
            }
            catch when (attempt < 4)
            {
                Thread.Sleep(50);
            }
            catch
            {
                return;
            }
        }
    }
}
