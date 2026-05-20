namespace CsStratware.Core.Models;

/// <summary>Root mod.json descriptor for a UE4 mod package.</summary>
public sealed class ModManifest
{
    public const int SchemaVersion = 1;

    public required string Id { get; init; }
    public required string Name { get; init; }
    public required string Version { get; init; }
    public string? Description { get; init; }
    public string? Author { get; init; }
    public Ue4Target? Target { get; init; }
    public IReadOnlyList<ModDependency> Dependencies { get; init; } = [];
    /// <summary>Mod ids that must load before this mod (same as soft dependencies).</summary>
    public IReadOnlyList<string> LoadAfter { get; init; } = [];
    /// <summary>Mod ids that must load after this mod.</summary>
    public IReadOnlyList<string> LoadBefore { get; init; } = [];
    /// <summary>Mod ids that cannot be active together with this mod.</summary>
    public IReadOnlyList<string> IncompatibleWith { get; init; } = [];
    public IReadOnlyList<string> ContentRoots { get; init; } = ["content"];
    public IReadOnlyList<string> PatchFiles { get; init; } = [];
    /// <summary>Relative path to mod .csproj (e.g. code/MyMod.csproj). If omitted, single code/*.csproj is used.</summary>
    public string? CodeProject { get; init; }
    public ModPakSettings? Pak { get; init; }
}

public sealed class ModPakSettings
{
    public string? Output { get; init; }
    public string? MountPoint { get; init; }
    public string? SourcePak { get; init; }
    public string? SourceFilter { get; init; }
    public bool UseUnrealPak { get; init; }
}

public sealed class Ue4Target
{
    public string? GameId { get; init; }
    public string? EngineVersion { get; init; }
    public string? MinGameVersion { get; init; }
    public string? MaxGameVersion { get; init; }
}

public sealed class ModDependency
{
    public required string Id { get; init; }
    public string? Version { get; init; }
    public bool Optional { get; init; }
}
