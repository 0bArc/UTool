namespace UTool.Sdk;

/// <summary>Target save file while applying a <see cref="PlayerDataPatch"/>.</summary>
public sealed class PlayerDataApplyContext
{
    public required string ProfileId { get; init; }
    public required string RelativePath { get; init; }
    public required string FullPath { get; init; }
    public required IPlayerSaveContext Saves { get; init; }
}
