using System.Text.Json;
using UTool.Core.Json;

namespace UTool.ModLoader;

public static class AssetPatchReader
{
    public static AssetPatchDocument Read(string patchFilePath)
    {
        var json = File.ReadAllText(patchFilePath);
        var doc = JsonSerializer.Deserialize<AssetPatchDocument>(json, UToolJson.Options)
            ?? throw new InvalidOperationException($"Invalid patch file: {patchFilePath}");
        return doc;
    }
}
