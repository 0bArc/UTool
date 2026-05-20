namespace CsStratware.Sdk;

[AttributeUsage(AttributeTargets.Class, Inherited = false)]
public sealed class PatchPlayerDataAttribute(string relativePath) : Attribute
{
    /// <summary>Path under each profile folder, e.g. Accolades.json or Loadout/Loadouts.json.</summary>
    public string RelativePath { get; } = relativePath;
}
