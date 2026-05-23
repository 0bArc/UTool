namespace UTool.Sdk;

/// <summary>Mutable CurveFloat key list for <see cref="CurvePatch"/> code mods.</summary>
public sealed class CurveEditor
{
    private readonly List<CurveKey> _keys = [];

    public CurveEditor(string assetName, IEnumerable<CurveKey>? vanillaKeys = null)
    {
        AssetName = assetName;
        if (vanillaKeys is not null)
        {
            foreach (var key in vanillaKeys.OrderBy(k => k.Time))
                _keys.Add(key);
        }
    }

    public string AssetName { get; }

    public IReadOnlyList<CurveKey> Keys => _keys;

    public CurveKey LastKey()
    {
        if (_keys.Count == 0)
            throw new InvalidOperationException($"Curve '{AssetName}' has no keys.");

        return _keys.MaxBy(k => k.Time);
    }

    public void AddKey(float time, float value)
    {
        var idx = _keys.FindIndex(k => Math.Abs(k.Time - time) < 1e-4f);
        if (idx >= 0)
            _keys[idx] = new CurveKey(time, value);
        else
            _keys.Add(new CurveKey(time, value));
    }

    public void SetKey(float time, float value) => AddKey(time, value);
}
