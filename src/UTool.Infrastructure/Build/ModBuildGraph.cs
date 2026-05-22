namespace UTool.Infrastructure.Build;

public sealed class ModBuildNode
{
    public required string Id { get; init; }
    public IReadOnlyList<string> DependsOn { get; init; } = [];
    public Func<CancellationToken, Task>? ExecuteAsync { get; init; }
}

public static class ModBuildGraph
{
    public static async Task RunAsync(
        IReadOnlyList<ModBuildNode> nodes,
        CancellationToken cancellationToken = default)
    {
        var pending = new HashSet<string>(nodes.Select(n => n.Id), StringComparer.OrdinalIgnoreCase);
        var completed = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var map = nodes.ToDictionary(n => n.Id, StringComparer.OrdinalIgnoreCase);

        while (pending.Count > 0)
        {
            cancellationToken.ThrowIfCancellationRequested();
            var ready = pending
                .Where(id => map[id].DependsOn.All(d => completed.Contains(d)))
                .ToList();

            if (ready.Count == 0)
                throw new InvalidOperationException("Mod build graph has a cycle or missing dependency.");

            await Task.WhenAll(ready.Select(async id =>
            {
                var node = map[id];
                if (node.ExecuteAsync is not null)
                    await node.ExecuteAsync(cancellationToken).ConfigureAwait(false);
                lock (completed)
                {
                    completed.Add(id);
                    pending.Remove(id);
                }
            })).ConfigureAwait(false);
        }
    }
}
