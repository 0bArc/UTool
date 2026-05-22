namespace UTool.Sdk;

/// <summary>Asset patch that runs only when <see cref="ShouldApply"/> is true (e.g. boss defeated in local saves).</summary>
public abstract class ConditionalAssetPatch : AssetPatch
{
    public virtual bool ShouldApply(IPlayerSaveContext saves) => true;
}
