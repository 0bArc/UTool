namespace UTool.Pak;

public sealed class PakMergeOptions
{
    public PakOpenOptions? PakOpenOptions { get; init; }

    /// <summary>When true, colliding .json entries are merged (UE Rows union) instead of last-wins.</summary>
    public bool JsonMerge { get; init; } = true;

    public PakBuildOptions? BuildOptions { get; init; }

    /// <summary>Repack via UnrealPak when available (required for UE pak v10+ / Icarus mods).</summary>
    public bool PreferUnrealPak { get; init; } = true;

    public UnrealPakOptions? UnrealPakOptions { get; init; }

    /// <summary>Override extract root; default is <c>EXTRACTED-MOD/FILES</c> beside output or shared pak folder.</summary>
    public string? FilesDirectory { get; init; }

    /// <summary>When true, wipe <see cref="FilesDirectory"/> before extracting source paks.</summary>
    public bool ClearExtractedDirectory { get; init; } = true;

    /// <summary>Staging overwrite / JSON merge diagnostics (e.g. CLI <c>--verbose</c>).</summary>
    public Action<string>? Log { get; init; }
}
