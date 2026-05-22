using UTool.Sdk;

/// <summary>Example code patch — wire asset path to your exported JSON file name.</summary>
[PatchAsset("ExampleGameplay.json")]
public sealed class GameplayPatch : AssetPatch
{
    public override void Apply(JsonAssetEditor editor)
    {
        editor.Replace("/difficulty", "normal");
    }
}
