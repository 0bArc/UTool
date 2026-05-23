namespace UTool.Sdk;

[AttributeUsage(AttributeTargets.Class, AllowMultiple = true, Inherited = false)]
public sealed class PatchCurveAttribute(
    string assetName,
    string relativeDirectory = "Character",
    bool extendFromVanilla = true) : Attribute
{
    public string AssetName { get; } = assetName;

    public string RelativeDirectory { get; } = relativeDirectory;

    public bool ExtendFromVanilla { get; } = extendFromVanilla;
}
