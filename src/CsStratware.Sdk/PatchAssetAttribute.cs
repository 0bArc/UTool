namespace CsStratware.Sdk;

[AttributeUsage(AttributeTargets.Class, Inherited = false)]
public sealed class PatchAssetAttribute(string assetFileName) : Attribute
{
    public string AssetFileName { get; } = assetFileName;
}
