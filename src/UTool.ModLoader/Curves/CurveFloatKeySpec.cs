namespace UTool.ModLoader.Curves;

public sealed class CurveFloatKeySpec
{
    public float Time { get; init; }
    public float Value { get; init; }
}

public sealed class CurveFloatPatchSpec
{
    public required string AssetName { get; init; }
    public string RelativeDirectory { get; init; } = "Character";
    public IReadOnlyList<CurveFloatKeySpec> Keys { get; init; } = [];
    public bool ExtendFromVanilla { get; init; } = true;
    public float? MinPatchTime { get; init; }
}
