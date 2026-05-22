using System.Text.Json;
using UTool.Core.Json;
using UTool.Core.Models;

namespace UTool.ModLoader;

public static class ModManifestReader
{
    public const string ManifestFileName = "mod.json";

    public static async Task<ModManifest> ReadAsync(string manifestPath, CancellationToken cancellationToken = default)
    {
        await using var stream = File.OpenRead(manifestPath);
        var manifest = await JsonSerializer.DeserializeAsync<ModManifest>(stream, UToolJson.Options, cancellationToken)
            ?? throw new InvalidOperationException($"Failed to deserialize manifest: {manifestPath}");

        Validate(manifest, manifestPath);
        return manifest;
    }

    public static void Validate(ModManifest manifest, string? contextPath = null)
    {
        if (string.IsNullOrWhiteSpace(manifest.Id))
            throw new InvalidOperationException(Format("Mod id is required.", contextPath));

        if (string.IsNullOrWhiteSpace(manifest.Name))
            throw new InvalidOperationException(Format("Mod name is required.", contextPath));

        if (string.IsNullOrWhiteSpace(manifest.Version))
            throw new InvalidOperationException(Format("Mod version is required.", contextPath));
    }

    private static string Format(string message, string? path) =>
        path is null ? message : $"{message} ({path})";
}
