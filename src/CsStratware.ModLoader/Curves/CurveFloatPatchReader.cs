using System.Text.Json;
using CsStratware.Core.Json;

namespace CsStratware.ModLoader.Curves;

public static class CurveFloatPatchReader
{
    public static IReadOnlyList<CurveFloatPatchSpec> ReadDirectory(string curvesDir)
    {
        if (!Directory.Exists(curvesDir))
            return [];

        var specs = new List<CurveFloatPatchSpec>();
        foreach (var path in Directory.EnumerateFiles(curvesDir, "*.curve.json", SearchOption.TopDirectoryOnly))
        {
            var json = File.ReadAllText(path);
            var spec = JsonSerializer.Deserialize<CurveFloatPatchSpec>(json, StratwareJson.Options)
                ?? throw new InvalidOperationException($"Failed to deserialize curve patch: {path}");

            if (string.IsNullOrWhiteSpace(spec.AssetName))
                throw new InvalidOperationException($"Curve patch missing assetName: {path}");

            if (spec.Keys.Count == 0)
                throw new InvalidOperationException($"Curve patch has no keys: {path}");

            specs.Add(spec);
        }

        return specs;
    }
}
