using UTool.ModLoader.Curves;
using UTool.Sdk;
using Xunit;

namespace UTool.Tests;

public sealed class CurveCodePatchRunnerTests
{
    [Fact]
    public void BuildSpec_appends_keys_after_vanilla_max()
    {
        var vanilla = Enumerable.Range(1, 60)
            .Select(l => new CurveKey(l, l * 10f))
            .ToList();

        var patch = new CodeCurvePatch
        {
            AssetName = "C_TestGrowth",
            Instance = new TestCurvePatch(),
        };

        var spec = CurveCodePatchRunner.BuildSpec(vanilla, patch);

        Assert.True(spec.ExtendFromVanilla);
        Assert.Equal(61f, spec.MinPatchTime);
        Assert.Equal(3, spec.Keys.Count);
        Assert.Equal(61f, spec.Keys[0].Time);
        Assert.Equal(610f, spec.Keys[0].Value);
    }

    private sealed class TestCurvePatch : CurvePatch
    {
        public override void Apply(CurveEditor curve)
        {
            var last = curve.LastKey();
            var value = last.Value;
            for (var level = (int)last.Time + 1; level <= 63; level++)
            {
                value += 10f;
                curve.AddKey(level, value);
            }
        }
    }
}
