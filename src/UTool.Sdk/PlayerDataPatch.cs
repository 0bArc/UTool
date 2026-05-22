namespace UTool.Sdk;

/// <summary>Patch a JSON file under Saved/PlayerData/&lt;profileId&gt;/.</summary>
public abstract class PlayerDataPatch
{
    public abstract void Apply(JsonAssetEditor editor, PlayerDataApplyContext context);
}
