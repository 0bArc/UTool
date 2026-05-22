namespace UTool.ModLoader.Merge;

/// <summary>Atomic JSON write with backup + restore on failure.</summary>
public static class SafeJsonFileWriter
{
    public const string BackupExtension = ".csmerge.bak";

    public static void Write(string targetPath, string json, bool keepBackup = true)
    {
        targetPath = Path.GetFullPath(targetPath);
        var directory = Path.GetDirectoryName(targetPath);
        if (!string.IsNullOrEmpty(directory))
            Directory.CreateDirectory(directory);

        var backupPath = targetPath + BackupExtension;
        var tempPath = targetPath + ".csmerge.tmp";
        string? previousBackup = null;

        try
        {
            if (File.Exists(targetPath))
            {
                if (File.Exists(backupPath))
                    previousBackup = TryReadAllText(backupPath);

                File.Copy(targetPath, backupPath, overwrite: true);
            }

            File.WriteAllText(tempPath, json);
            if (File.Exists(targetPath))
                File.Replace(tempPath, targetPath, backupPath);
            else
            {
                File.Move(tempPath, targetPath);
                if (!keepBackup && File.Exists(backupPath))
                    File.Delete(backupPath);
            }
        }
        catch
        {
            TryRestore(targetPath, backupPath, tempPath, previousBackup);
            throw;
        }
        finally
        {
            if (File.Exists(tempPath))
            {
                try { File.Delete(tempPath); } catch { /* ignore */ }
            }

            if (!keepBackup && File.Exists(backupPath))
            {
                try { File.Delete(backupPath); } catch { /* ignore */ }
            }
        }
    }

    private static void TryRestore(
        string targetPath,
        string backupPath,
        string tempPath,
        string? previousBackup)
    {
        try
        {
            if (File.Exists(tempPath))
                File.Delete(tempPath);

            if (File.Exists(backupPath))
            {
                File.Copy(backupPath, targetPath, overwrite: true);
                if (previousBackup is not null)
                    File.WriteAllText(backupPath, previousBackup);
            }
        }
        catch
        {
            // Best-effort restore; caller sees original exception.
        }
    }

    private static string? TryReadAllText(string path)
    {
        try { return File.ReadAllText(path); }
        catch { return null; }
    }
}
