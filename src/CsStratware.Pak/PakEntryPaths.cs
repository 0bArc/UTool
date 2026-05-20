namespace CsStratware.Pak;

/// <summary>Normalize pak entry paths for cross-pak comparison and merge.</summary>
public static class PakEntryPaths
{
    public static string ToRelativePath(string entryPath, string mountPoint)
    {
        if (entryPath.StartsWith(mountPoint, StringComparison.OrdinalIgnoreCase))
            return entryPath[mountPoint.Length..].TrimStart('/', '\\');

        return entryPath.TrimStart('/', '\\');
    }

    public static string FileNameFromRelative(string relativePath) =>
        relativePath.Replace('\\', '/').Split('/').LastOrDefault() ?? relativePath;
}
