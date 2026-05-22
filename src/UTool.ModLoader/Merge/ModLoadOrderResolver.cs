using UTool.Core.Models;

namespace UTool.ModLoader.Merge;

public sealed class ModLoadOrderIssue
{
    public required ModIssueSeverity Severity { get; init; }
    public string? ModId { get; init; }
    public required string Message { get; init; }
}

public sealed class ModLoadOrderResult
{
    public required IReadOnlyList<ModPackage> OrderedMods { get; init; }
    public required IReadOnlyList<ModLoadOrderIssue> Issues { get; init; }
    public bool Success => !Issues.Any(i => i.Severity == ModIssueSeverity.Error);
}

/// <summary>Topological mod load order: dependencies, loadAfter, loadBefore.</summary>
public static class ModLoadOrderResolver
{
    public static ModLoadOrderResult Resolve(IReadOnlyList<ModPackage> mods)
    {
        var issues = new List<ModLoadOrderIssue>();
        var byId = mods.ToDictionary(m => m.Manifest.Id, StringComparer.OrdinalIgnoreCase);

        foreach (var mod in mods)
        {
            foreach (var otherId in mod.Manifest.IncompatibleWith)
            {
                if (!byId.ContainsKey(otherId))
                    continue;
                issues.Add(new ModLoadOrderIssue
                {
                    Severity = ModIssueSeverity.Error,
                    ModId = mod.Manifest.Id,
                    Message = $"Incompatible with '{otherId}' (both mods present).",
                });
            }
        }

        var edges = new Dictionary<string, HashSet<string>>(StringComparer.OrdinalIgnoreCase);
        foreach (var mod in mods)
        {
            if (!edges.ContainsKey(mod.Manifest.Id))
                edges[mod.Manifest.Id] = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

            foreach (var dep in mod.Manifest.Dependencies.Where(d => !d.Optional))
            {
                if (!byId.ContainsKey(dep.Id))
                {
                    issues.Add(new ModLoadOrderIssue
                    {
                        Severity = ModIssueSeverity.Error,
                        ModId = mod.Manifest.Id,
                        Message = $"Missing required dependency: {dep.Id}",
                    });
                    continue;
                }

                AddEdge(edges, dep.Id, mod.Manifest.Id);
            }

            foreach (var afterId in mod.Manifest.LoadAfter)
            {
                if (!byId.ContainsKey(afterId))
                {
                    issues.Add(new ModLoadOrderIssue
                    {
                        Severity = ModIssueSeverity.Warning,
                        ModId = mod.Manifest.Id,
                        Message = $"loadAfter '{afterId}' not found in mod set.",
                    });
                    continue;
                }

                AddEdge(edges, afterId, mod.Manifest.Id);
            }

            foreach (var beforeId in mod.Manifest.LoadBefore)
            {
                if (!byId.ContainsKey(beforeId))
                {
                    issues.Add(new ModLoadOrderIssue
                    {
                        Severity = ModIssueSeverity.Warning,
                        ModId = mod.Manifest.Id,
                        Message = $"loadBefore '{beforeId}' not found in mod set.",
                    });
                    continue;
                }

                AddEdge(edges, mod.Manifest.Id, beforeId);
            }
        }

        var inDegree = mods.ToDictionary(m => m.Manifest.Id, _ => 0, StringComparer.OrdinalIgnoreCase);
        foreach (var (from, toSet) in edges)
        {
            foreach (var to in toSet)
                inDegree[to] = inDegree.GetValueOrDefault(to) + 1;
        }

        var queue = new PriorityQueue<string, string>(Comparer<string>.Create(StringComparer.OrdinalIgnoreCase.Compare));
        foreach (var mod in mods)
        {
            if (inDegree[mod.Manifest.Id] == 0)
                queue.Enqueue(mod.Manifest.Id, mod.Manifest.Id);
        }

        var orderedIds = new List<string>();
        while (queue.Count > 0)
        {
            var id = queue.Dequeue();
            orderedIds.Add(id);

            if (!edges.TryGetValue(id, out var dependents))
                continue;

            foreach (var depId in dependents.OrderBy(x => x, StringComparer.OrdinalIgnoreCase))
            {
                inDegree[depId]--;
                if (inDegree[depId] == 0)
                    queue.Enqueue(depId, depId);
            }
        }

        if (orderedIds.Count != mods.Count)
        {
            issues.Add(new ModLoadOrderIssue
            {
                Severity = ModIssueSeverity.Error,
                Message = "Circular mod load order (dependencies / loadAfter / loadBefore).",
            });
            orderedIds = mods.Select(m => m.Manifest.Id).OrderBy(x => x, StringComparer.OrdinalIgnoreCase).ToList();
        }

        var ordered = orderedIds
            .Where(byId.ContainsKey)
            .Select(id => byId[id])
            .ToList();

        return new ModLoadOrderResult { OrderedMods = ordered, Issues = issues };
    }

    private static void AddEdge(Dictionary<string, HashSet<string>> edges, string from, string to)
    {
        if (!edges.TryGetValue(from, out var set))
        {
            set = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            edges[from] = set;
        }

        set.Add(to);
    }
}
