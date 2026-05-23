using UTool.Sdk;

[PatchAsset("D_CharacterGrowth.json")]
public sealed class Level250CapGrowthPatch : AssetPatch
{
    public const int MaxLevel = 250;

    public override void Apply(JsonAssetEditor editor)
    {
        editor.SetOnArrayElementsWhere(
            "/Rows",
            "Name",
            "Player",
            "/MaxDisplayLevel",
            MaxLevel);

        editor.SetOnArrayElementsWhere(
            "/Rows",
            "Name",
            "Player",
            "/MaxLevel",
            MaxLevel);
    }
}
