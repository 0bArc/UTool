using System.Text.Json;
using CsStratware.Core.Json;

namespace CsStratware.ModLoader;

public static class AssetPatchReader
{
    public static AssetPatchDocument Read(string patchFilePath)
    {
        var json = File.ReadAllText(patchFilePath);
        var doc = JsonSerializer.Deserialize<AssetPatchDocument>(json, StratwareJson.Options)
            ?? throw new InvalidOperationException($"Invalid patch file: {patchFilePath}");
        return doc;
    }
}
