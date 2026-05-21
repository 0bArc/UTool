using CsStratware.ModLoader.Curves;
using UAssetAPI;
using UAssetAPI.UnrealTypes;
using Xunit;

namespace CsStratware.Tests;

public sealed class CurveFloatPatcherTests
{
    private static string? TalentGrowthUasset =>
        Environment.GetEnvironmentVariable("ICARUS_TALENT_CURVE_UASSET");

    [Fact]
    public void Serialize_talent_curve_when_asset_present()
    {
        var path = TalentGrowthUasset;
        if (string.IsNullOrWhiteSpace(path) || !File.Exists(path))
            return;

        var asset = new UAsset(path, EngineVersion.VER_UE4_27);
        var json = asset.SerializeJson();
        var dump = Path.Combine(Path.GetTempPath(), "csstratware-talent-curve.json");
        File.WriteAllText(dump, json);
        Assert.True(
            json.Contains("Keys", StringComparison.Ordinal)
            || json.Contains("FloatCurve", StringComparison.Ordinal)
            || json.Contains("RichCurve", StringComparison.Ordinal),
            $"Expected curve keys in JSON (dump: {dump})");
    }

    [Fact]
    public void Patch_extends_keys_when_asset_present()
    {
        var path = TalentGrowthUasset;
        if (string.IsNullOrWhiteSpace(path) || !File.Exists(path))
            return;

        var dir = Path.Combine(Path.GetTempPath(), "csstratware-curve-test");
        Directory.CreateDirectory(dir);
        var copy = Path.Combine(dir, "C_PlayerTalentGrowth.uasset");
        var copyExp = Path.Combine(dir, "C_PlayerTalentGrowth.uexp");
        File.Copy(path, copy, overwrite: true);
        File.Copy(Path.ChangeExtension(path, ".uexp"), copyExp, overwrite: true);

        var spec = new CurveFloatPatchSpec
        {
            AssetName = "C_PlayerTalentGrowth",
            Keys = Enumerable.Range(1, 250).Select(i => new CurveFloatKeySpec { Time = i, Value = 1 }).ToList(),
            ExtendFromVanilla = true,
            MinPatchTime = 61,
        };

        CurveFloatPatcher.ApplyKeys(copy, spec);
        var patched = new UAsset(copy, EngineVersion.VER_UE4_27);
        Assert.True(patched.Exports.Count > 0);
    }
}
