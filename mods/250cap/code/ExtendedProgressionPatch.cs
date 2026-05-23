using UTool.Sdk;

[PatchCurve("C_PlayerExperienceGrowth")]
[PatchCurve("C_PlayerTalentGrowth")]
[PatchCurve("C_PlayerBlueprintGrowth")]
[PatchCurve("C_SoloTalentGrowth")]
public sealed class ExtendedProgressionPatch : CurvePatch
{
    private const int MaxLevel = 250;
    private const int XpPerLevel = 144_000;
    private const float TalentPointsPerLevel = 2f;
    private const float BlueprintPointsPerLevel = 1f;
    private const float SoloPointsPerLevel = 1f;

    public override void Apply(CurveEditor curve)
    {
        switch (curve.AssetName)
        {
            case "C_PlayerExperienceGrowth":
                ExtendXpCurve(curve);
                break;
            case "C_PlayerTalentGrowth":
                ExtendLinearCurve(curve, TalentPointsPerLevel);
                break;
            case "C_PlayerBlueprintGrowth":
                ExtendLinearCurve(curve, BlueprintPointsPerLevel);
                break;
            case "C_SoloTalentGrowth":
                ExtendLinearCurve(curve, SoloPointsPerLevel);
                break;
        }
    }

    private static void ExtendXpCurve(CurveEditor curve)
    {
        var last = curve.LastKey();
        var xp = last.Value;

        for (var level = (int)last.Time + 1; level <= MaxLevel; level++)
        {
            xp += XpPerLevel;
            curve.AddKey(level, xp);
        }
    }

    private static void ExtendLinearCurve(CurveEditor curve, float gainPerLevel)
    {
        var last = curve.LastKey();
        var value = last.Value;

        for (var level = (int)last.Time + 1; level <= MaxLevel; level++)
        {
            value += gainPerLevel;
            curve.AddKey(level, value);
        }
    }
}
