namespace UTool.Sdk;

/// <summary>Subclass and implement <see cref="Apply"/> to patch a UE CurveFloat asset in code.</summary>
public abstract class CurvePatch
{
    public abstract void Apply(CurveEditor curve);
}
