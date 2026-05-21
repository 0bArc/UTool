namespace CsStratware.Pak;

/// <summary>Paths for extract → merge JSON → repack workflow.</summary>
public static class PakMergePaths
{
    public const string ExtractedRootFolder = "EXTRACTED-MOD";
    public const string ExtractedFilesFolder = "FILES";

    /// <summary><c>EXTRACTED-MOD/FILES</c> next to output pak, or shared parent of all source paks.</summary>
    public static string ResolveFilesDirectory(string outputPakPath, IReadOnlyList<string> sourcePakPaths)
    {
        var outputDir = Path.GetDirectoryName(Path.GetFullPath(outputPakPath))
            ?? throw new ArgumentException("Output pak path must include a directory.", nameof(outputPakPath));

        var sourceDirs = sourcePakPaths
            .Select(p => Path.GetDirectoryName(Path.GetFullPath(p)))
            .Where(d => !string.IsNullOrWhiteSpace(d))
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .ToList();

        var root = sourceDirs.Count == 1 ? sourceDirs[0]! : outputDir;
        return Path.Combine(root, ExtractedRootFolder, ExtractedFilesFolder);
    }

    public static void PrepareFilesDirectory(string filesDirectory, bool clearExisting)
    {
        filesDirectory = Path.GetFullPath(filesDirectory);
        if (clearExisting && Directory.Exists(filesDirectory))
        {
            try { Directory.Delete(filesDirectory, recursive: true); } catch { /* best effort */ }
        }

        Directory.CreateDirectory(filesDirectory);
    }
}
