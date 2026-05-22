namespace UTool.Pak;

/// <summary>Normalize pak entry paths for cross-pak comparison and merge.</summary>
public static class PakEntryPaths
{
    public static string NormalizeMountPoint(string mountPoint)
    {
        mountPoint = mountPoint.Replace('\\', '/');
        if (!mountPoint.EndsWith('/'))
            mountPoint += '/';
        return mountPoint;
    }

    public static string CommonMountPoint(IEnumerable<string> mountPoints)
    {
        var parts = mountPoints
            .Where(m => !string.IsNullOrWhiteSpace(m))
            .Select(m => NormalizeMountPoint(m).TrimEnd('/').Split('/'))
            .ToList();

        if (parts.Count == 0)
            return "";

        var common = new List<string>();
        for (var i = 0; i < parts[0].Length; i++)
        {
            var candidate = parts[0][i];
            if (parts.Any(p => i >= p.Length || !string.Equals(p[i], candidate, StringComparison.OrdinalIgnoreCase)))
                break;

            common.Add(candidate);
        }

        if (common.Count == 0)
            return NormalizeMountPoint(string.Join('/', parts[0]));

        return NormalizeMountPoint(string.Join('/', common));
    }

    public static string ToRelativePath(string entryPath, string mountPoint)
    {
        mountPoint = NormalizeMountPoint(mountPoint);
        if (entryPath.StartsWith(mountPoint, StringComparison.OrdinalIgnoreCase))
            return entryPath[mountPoint.Length..].TrimStart('/', '\\');

        return entryPath.TrimStart('/', '\\');
    }

    public static string FileNameFromRelative(string relativePath) =>
        relativePath.Replace('\\', '/').Split('/').LastOrDefault() ?? relativePath;
}
