namespace UTool.Pak;

internal static class PakPathPattern
{
    public static bool Matches(string entryPath, string? pattern, bool ignoreCase = true)
    {
        if (string.IsNullOrWhiteSpace(pattern))
            return true;

        var comparison = ignoreCase ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal;
        var needle = pattern.Trim();
        if (needle is "*" or "**")
            return true;

        while (needle.StartsWith('*'))
            needle = needle[1..];
        while (needle.EndsWith('*'))
            needle = needle[..^1];

        if (needle.Length == 0)
            return true;

        return entryPath.Contains(needle, comparison);
    }
}
