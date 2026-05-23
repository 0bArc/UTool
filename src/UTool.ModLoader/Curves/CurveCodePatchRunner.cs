using UTool.Sdk;

namespace UTool.ModLoader.Curves;

public sealed class CodeCurvePatch
{
    public required string AssetName { get; init; }
    public string RelativeDirectory { get; init; } = "Character";
    public bool ExtendFromVanilla { get; init; } = true;
    public required CurvePatch Instance { get; init; }
}

public static class CurveCodePatchRunner
{
    public static CurveFloatPatchSpec BuildSpecFromUasset(string uassetPath, CodeCurvePatch patch)
    {
        var vanilla = CurveFloatVanillaLoader.ReadKeys(uassetPath);
        return BuildSpec(vanilla, patch);
    }

    public static CurveFloatPatchSpec BuildSpec(
        IReadOnlyList<CurveKey> vanillaKeys,
        CodeCurvePatch patch)
    {
        var assetName = patch.AssetName.EndsWith(".uasset", StringComparison.OrdinalIgnoreCase)
            ? patch.AssetName[..^7]
            : patch.AssetName;

        var editor = new CurveEditor(assetName, vanillaKeys);
        var vanillaMaxTime = vanillaKeys.Count == 0 ? float.NegativeInfinity : vanillaKeys.Max(k => k.Time);
        patch.Instance.Apply(editor);

        var exportKeys = patch.ExtendFromVanilla
            ? editor.Keys
                .Where(k => k.Time > vanillaMaxTime + 1e-4f)
                .OrderBy(k => k.Time)
                .Select(k => new CurveFloatKeySpec { Time = k.Time, Value = k.Value })
                .ToList()
            : editor.Keys
                .OrderBy(k => k.Time)
                .Select(k => new CurveFloatKeySpec { Time = k.Time, Value = k.Value })
                .ToList();

        if (exportKeys.Count == 0)
            throw new InvalidOperationException($"Curve patch '{patch.Instance.GetType().Name}' produced no keys for {assetName}.");

        var minPatch = patch.ExtendFromVanilla
            ? exportKeys.Min(k => k.Time)
            : editor.Keys.Min(k => k.Time);

        return new CurveFloatPatchSpec
        {
            AssetName = assetName,
            RelativeDirectory = patch.RelativeDirectory,
            Keys = exportKeys,
            ExtendFromVanilla = patch.ExtendFromVanilla,
            MinPatchTime = minPatch,
        };
    }
}
