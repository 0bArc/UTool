using System.Text.Json;
using System.Text.Json.Serialization;

namespace CsStratware.Cli;

public sealed class StratwareConfig
{
    public string? ConfigDirectory { get; private set; }
    [JsonPropertyName("unrealPak")]
    public string? UnrealPak { get; init; }

    [JsonPropertyName("icarusPaksDir")]
    public string? IcarusPaksDir { get; init; }

    [JsonPropertyName("demoExtractedDir")]
    public string? DemoExtractedDir { get; init; }

    [JsonPropertyName("icarusMountPoint")]
    public string? IcarusMountPoint { get; init; }

    [JsonPropertyName("icarusDataPak")]
    public string? IcarusDataPak { get; init; }

    [JsonPropertyName("unrealEngineDir")]
    public string? UnrealEngineDir { get; init; }

    public string ResolveIcarusDataPak() =>
        IcarusDataPak
        ?? (string.IsNullOrWhiteSpace(IcarusPaksDir)
            ? ""
            : Path.GetFullPath(Path.Combine(IcarusPaksDir, "..", "Data", "data.pak")));

    public string? ResolveDemoExtractedDir()
    {
        if (string.IsNullOrWhiteSpace(DemoExtractedDir))
            return null;
        if (Path.IsPathRooted(DemoExtractedDir))
            return DemoExtractedDir;
        if (string.IsNullOrWhiteSpace(ConfigDirectory))
            return DemoExtractedDir;
        return Path.GetFullPath(Path.Combine(ConfigDirectory, DemoExtractedDir));
    }

    public static StratwareConfig Load(string? startDirectory = null)
    {
        var dir = startDirectory ?? Directory.GetCurrentDirectory();
        for (var i = 0; i < 8 && !string.IsNullOrEmpty(dir); i++)
        {
            var path = Path.Combine(dir, "csstratware.json");
            if (File.Exists(path))
            {
                var json = File.ReadAllText(path);
                var cfg = JsonSerializer.Deserialize<StratwareConfig>(json, JsonOptions)
                    ?? new StratwareConfig();
                cfg.ConfigDirectory = dir;
                return cfg;
            }

            var parent = Directory.GetParent(dir);
            dir = parent?.FullName;
        }

        return new StratwareConfig();
    }

    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNameCaseInsensitive = true,
        ReadCommentHandling = JsonCommentHandling.Skip,
        AllowTrailingCommas = true,
    };
}
