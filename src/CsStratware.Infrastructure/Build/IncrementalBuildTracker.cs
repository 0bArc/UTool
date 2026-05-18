using System.Text.Json;
using CsStratware.Core.Json;
using CsStratware.Infrastructure.Caching;

namespace CsStratware.Infrastructure.Build;

public sealed class BuildInputRecord
{
    public string Path { get; set; } = "";
    public string Sha256 { get; set; } = "";
}

public sealed class BuildOutputRecord
{
    public string Path { get; set; } = "";
    public string Sha256 { get; set; } = "";
}

public sealed class IncrementalBuildState
{
    public string Stage { get; set; } = "";
    public List<BuildInputRecord> Inputs { get; set; } = [];
    public List<BuildOutputRecord> Outputs { get; set; } = [];
}

public sealed class IncrementalBuildTracker
{
    private readonly string _statePath;

    public IncrementalBuildTracker(string modRoot, string stage)
    {
        _statePath = Path.Combine(modRoot, ".cache", $"build-{stage}.json");
    }

    public bool IsUpToDate(IReadOnlyDictionary<string, string> inputHashes, IReadOnlyList<string> outputPaths)
    {
        if (!File.Exists(_statePath) || outputPaths.Count == 0)
            return false;

        try
        {
            var state = Load();
            if (!string.Equals(state.Stage, Path.GetFileName(_statePath), StringComparison.OrdinalIgnoreCase)
                && state.Inputs.Count != inputHashes.Count)
            {
                // Stage name mismatch is ok; compare hashes.
            }

            foreach (var (path, hash) in inputHashes)
            {
                var recorded = state.Inputs.FirstOrDefault(i =>
                    string.Equals(i.Path, path, StringComparison.OrdinalIgnoreCase));
                if (recorded is null || !string.Equals(recorded.Sha256, hash, StringComparison.OrdinalIgnoreCase))
                    return false;
            }

            foreach (var output in outputPaths)
            {
                if (!File.Exists(output))
                    return false;

                var recorded = state.Outputs.FirstOrDefault(o =>
                    string.Equals(o.Path, output, StringComparison.OrdinalIgnoreCase));
                if (recorded is null)
                    return false;

                if (!string.Equals(recorded.Sha256, ContentHasher.HashFile(output), StringComparison.OrdinalIgnoreCase))
                    return false;
            }

            return true;
        }
        catch
        {
            return false;
        }
    }

    public void Record(string stage, IReadOnlyDictionary<string, string> inputHashes, IReadOnlyList<string> outputPaths)
    {
        var state = new IncrementalBuildState
        {
            Stage = stage,
            Inputs = inputHashes.Select(kv => new BuildInputRecord { Path = kv.Key, Sha256 = kv.Value }).ToList(),
            Outputs = outputPaths.Select(p => new BuildOutputRecord
            {
                Path = p,
                Sha256 = ContentHasher.HashFile(p),
            }).ToList(),
        };

        Directory.CreateDirectory(Path.GetDirectoryName(_statePath)!);
        File.WriteAllText(_statePath, JsonSerializer.Serialize(state, StratwareJson.Options));
    }

    private IncrementalBuildState Load()
    {
        var json = File.ReadAllText(_statePath);
        return JsonSerializer.Deserialize<IncrementalBuildState>(json, StratwareJson.Options)
               ?? new IncrementalBuildState();
    }
}
