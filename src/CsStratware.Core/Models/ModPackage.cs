namespace CsStratware.Core.Models;

/// <summary>Resolved mod on disk with manifest and root path.</summary>
public sealed class ModPackage
{
    public required string RootPath { get; init; }
    public required ModManifest Manifest { get; init; }
    public string ManifestPath { get; init; } = string.Empty;
}
