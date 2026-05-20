namespace CsStratware.Sdk;

/// <summary>Read-only view of UE4 local save folders under Saved/PlayerData.</summary>
public interface IPlayerSaveContext
{
    IReadOnlyList<string> ProfileIds { get; }

    /// <summary>True if <paramref name="rowName"/> appears in CompletedAccolades for any profile.</summary>
    bool AnyProfileHasCompletedAccolade(string rowName, string dataTableName = "D_Accolades");

    bool ProfileHasCompletedAccolade(string profileId, string rowName, string? dataTableName = "D_Accolades");

    string? FindProfileFile(string relativePath);

    bool ProfileFileExists(string profileId, string relativePath);
}
