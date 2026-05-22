namespace UTool.Sdk;

/// <summary>Subclass and implement <see cref="Apply"/> to patch a game JSON asset in code.</summary>
public abstract class AssetPatch
{
    public abstract void Apply(JsonAssetEditor editor);
}
