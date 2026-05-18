namespace CsStratware.Infrastructure.Mods;

public sealed class ModConflict
{
    public required string AssetPath { get; init; }
    public required string JsonPointer { get; init; }
    public required IReadOnlyList<string> Sources { get; init; }
}

public static class ModConflictResolver
{
    public static IReadOnlyList<ModConflict> DetectDuplicatePointerPatches(
        IEnumerable<(string Source, string AssetPath, IReadOnlyList<string> Pointers)> patchSets)
    {
        var map = new Dictionary<string, List<string>>(StringComparer.OrdinalIgnoreCase);

        foreach (var (source, asset, pointers) in patchSets)
        {
            foreach (var pointer in pointers)
            {
                var key = $"{asset}\0{pointer}";
                if (!map.TryGetValue(key, out var sources))
                {
                    sources = [];
                    map[key] = sources;
                }

                sources.Add(source);
            }
        }

        return map
            .Where(kv => kv.Value.Count > 1)
            .Select(kv =>
            {
                var parts = kv.Key.Split('\0', 2);
                return new ModConflict
                {
                    AssetPath = parts[0],
                    JsonPointer = parts.Length > 1 ? parts[1] : "",
                    Sources = kv.Value.Distinct(StringComparer.OrdinalIgnoreCase).ToList(),
                };
            })
            .ToList();
    }
}
