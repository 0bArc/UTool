using System.Text.Json;
using System.Text.Json.Serialization;
using CsStratware.Infrastructure.PlayerData;
using CsStratware.Pak;

namespace CsStratware.Cli;

public sealed class GameSettings
{
    [JsonPropertyName("paksDir")]
    public string? PaksDir { get; init; }

    [JsonPropertyName("dataPak")]
    public string? DataPak { get; init; }

    [JsonPropertyName("playerDataDir")]
    public string? PlayerDataDir { get; init; }

    [JsonPropertyName("mountPoint")]
    public string? MountPoint { get; init; }

    [JsonPropertyName("pakAesKey")]
    public string? PakAesKey { get; init; }
}

public sealed class StratwareConfig
{
    public string? ConfigDirectory { get; private set; }

    [JsonPropertyName("unrealPak")]
    public string? UnrealPak { get; init; }

    [JsonPropertyName("gamePaksDir")]
    public string? GamePaksDir { get; init; }

    [JsonPropertyName("dataPak")]
    public string? DataPak { get; init; }

    [JsonPropertyName("defaultMountPoint")]
    public string? DefaultMountPoint { get; init; }

    [JsonPropertyName("playerDataDir")]
    public string? PlayerDataDir { get; init; }

    [JsonPropertyName("extractedDir")]
    public string? ExtractedDir { get; init; }

    [JsonPropertyName("unrealEngineDir")]
    public string? UnrealEngineDir { get; init; }

    [JsonPropertyName("pakAesKey")]
    public string? PakAesKey { get; init; }

    [JsonPropertyName("games")]
    public Dictionary<string, GameSettings>? Games { get; init; }

    // Legacy csstratware.json keys (read-only aliases)
    [JsonPropertyName("icarusPaksDir")]
    public string? LegacyIcarusPaksDir { get; init; }

    [JsonPropertyName("icarusDataPak")]
    public string? LegacyIcarusDataPak { get; init; }

    [JsonPropertyName("icarusMountPoint")]
    public string? LegacyIcarusMountPoint { get; init; }

    [JsonPropertyName("icarusPlayerDataDir")]
    public string? LegacyIcarusPlayerDataDir { get; init; }

    [JsonPropertyName("demoExtractedDir")]
    public string? LegacyDemoExtractedDir { get; init; }

    public UnrealPakToolchainPaths ResolveUnrealPakToolchain(bool ensureLocalCopy = true) =>
        UnrealPakToolchain.Resolve(UnrealPak, UnrealEngineDir, ConfigDirectory, ensureLocalCopy);

    public string? ResolveSourcePak(string? token, string? gameId = null)
    {
        if (string.IsNullOrWhiteSpace(token))
            return null;

        return IsDataPakAlias(token)
            ? ResolveDataPak(gameId)
            : token;
    }

    public IReadOnlyList<string> ResolveSourcePakPaths(string? token, string? gameId = null)
    {
        if (string.IsNullOrWhiteSpace(token))
            return [];

        if (IsPaksDirAlias(token))
        {
            var paksDir = ResolvePaksDir(gameId)
                ?? throw new InvalidOperationException("paksDir not configured for @paks alias.");
            return PakPathResolver.Resolve(paksDir);
        }

        var single = ResolveSourcePak(token, gameId);
        return string.IsNullOrWhiteSpace(single) ? [] : [single];
    }

    public string? ResolvePaksDir(string? gameId = null)
    {
        var game = ResolveGame(gameId);
        var dir = game?.PaksDir ?? GamePaksDir ?? LegacyIcarusPaksDir;
        return string.IsNullOrWhiteSpace(dir) ? null : ResolvePath(dir);
    }

    public string ResolveDataPak(string? gameId = null)
    {
        var game = ResolveGame(gameId);
        var pak = game?.DataPak ?? DataPak ?? LegacyIcarusDataPak;
        if (!string.IsNullOrWhiteSpace(pak))
            return ResolvePath(pak);

        var paksDir = game?.PaksDir ?? GamePaksDir ?? LegacyIcarusPaksDir;
        if (!string.IsNullOrWhiteSpace(paksDir))
            return Path.GetFullPath(Path.Combine(ResolvePath(paksDir), "..", "Data", "data.pak"));

        throw new InvalidOperationException(
            "dataPak not configured. Set dataPak or gamePaksDir in csstratware.json, or mod.json pak.sourcePak to a file path.");
    }

    public string? ResolveMountPoint(string? gameId = null)
    {
        var game = ResolveGame(gameId);
        var mount = game?.MountPoint ?? DefaultMountPoint ?? LegacyIcarusMountPoint;
        return string.IsNullOrWhiteSpace(mount) ? null : mount;
    }

    public string ResolvePlayerDataDir(string? gameId = null)
    {
        var game = ResolveGame(gameId);
        var dir = game?.PlayerDataDir
            ?? PlayerDataDir
            ?? LegacyIcarusPlayerDataDir;
        if (!string.IsNullOrWhiteSpace(dir))
            return ResolvePath(dir);

        if (string.IsNullOrWhiteSpace(gameId))
        {
            throw new InvalidOperationException(
                "playerDataDir not configured and no gameId. Set playerDataDir in csstratware.json, games.<id>.playerDataDir, mod.json target.gameId, or CSSTRATWARE_PLAYER_DATA.");
        }

        return Ue4PlayerDataLocator.Resolve(gameId: gameId);
    }

    public byte[]? ResolvePakAesKey(string? gameId = null)
    {
        var game = ResolveGame(gameId);
        var material = game?.PakAesKey ?? PakAesKey;
        return PakOpenOptions.ParseAesKey(material);
    }

    public string? ResolveExtractedDir()
    {
        var dir = ExtractedDir ?? LegacyDemoExtractedDir;
        if (string.IsNullOrWhiteSpace(dir))
            return null;
        if (Path.IsPathRooted(dir))
            return dir;
        if (string.IsNullOrWhiteSpace(ConfigDirectory))
            return dir;
        return Path.GetFullPath(Path.Combine(ConfigDirectory, dir));
    }

    public static bool IsDataPakAlias(string token) =>
        token.Equals("@data", StringComparison.OrdinalIgnoreCase)
        || token.Equals("@game-data", StringComparison.OrdinalIgnoreCase)
        || token.Equals("@config:data", StringComparison.OrdinalIgnoreCase)
        || token.Equals("@icarus-data", StringComparison.OrdinalIgnoreCase);

    public static bool IsPaksDirAlias(string token) =>
        token.Equals("@paks", StringComparison.OrdinalIgnoreCase)
        || token.Equals("@game-paks", StringComparison.OrdinalIgnoreCase)
        || token.Equals("@config:paks", StringComparison.OrdinalIgnoreCase)
        || token.Equals("@icarus", StringComparison.OrdinalIgnoreCase);

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

    private GameSettings? ResolveGame(string? gameId)
    {
        if (string.IsNullOrWhiteSpace(gameId) || Games is null)
            return null;

        foreach (var (key, value) in Games)
        {
            if (key.Equals(gameId, StringComparison.OrdinalIgnoreCase))
                return value;
        }

        return null;
    }

    private string ResolvePath(string path) =>
        Path.IsPathRooted(path)
            ? Path.GetFullPath(path)
            : Path.GetFullPath(Path.Combine(ConfigDirectory ?? Directory.GetCurrentDirectory(), path));

    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNameCaseInsensitive = true,
        ReadCommentHandling = JsonCommentHandling.Skip,
        AllowTrailingCommas = true,
    };
}
