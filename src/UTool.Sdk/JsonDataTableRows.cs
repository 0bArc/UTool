using System.Text.Json.Nodes;
using UTool.Core.Json;

namespace UTool.Sdk;

/// <summary>Fluent helpers for UE DataTable <c>Rows</c> arrays.</summary>
public sealed class JsonDataTableRows
{
    private readonly JsonAssetEditor _editor;
    private readonly string _arrayPointer;
    private readonly string _nameProperty;
    private HashSet<string>? _nameFilter;

    internal JsonDataTableRows(JsonAssetEditor editor, string arrayPointer, string nameProperty = "Name")
    {
        _editor = editor;
        _arrayPointer = arrayPointer;
        _nameProperty = nameProperty;
    }

    public JsonDataTableRows WhereNameIn(params string[] names)
    {
        _nameFilter = new HashSet<string>(names, StringComparer.Ordinal);
        return this;
    }

    public JsonDataTableRows WhereNameIn(IEnumerable<string> names)
    {
        _nameFilter = new HashSet<string>(names, StringComparer.Ordinal);
        return this;
    }

    /// <summary>Multiply numeric property on matched rows (and optional defaults path).</summary>
    public int Scale(string propertyName, double factor, int minimum = 1)
    {
        var pointer = propertyName.StartsWith('/') ? propertyName : "/" + propertyName;
        var updated = 0;

        foreach (var row in EnumerateTargetRows())
        {
            if (!TryReadNumber(row, pointer.TrimStart('/'), out var current))
                continue;

            var scaled = Math.Max(minimum, (int)Math.Round(current * factor));
            _editor.SetOnRow(row, pointer, scaled);
            updated++;
        }

        return updated;
    }

    public int Set(string propertyName, object? value)
    {
        var pointer = propertyName.StartsWith('/') ? propertyName : "/" + propertyName;
        var updated = 0;
        foreach (var row in EnumerateTargetRows())
        {
            _editor.SetOnRow(row, pointer, value);
            updated++;
        }

        return updated;
    }

    private IEnumerable<JsonObject> EnumerateTargetRows() =>
        _editor.EnumerateArrayObjects(_arrayPointer)
            .Where(row => MatchesNameFilter(row));

    private bool MatchesNameFilter(JsonObject row)
    {
        if (_nameFilter is null || _nameFilter.Count == 0)
            return true;

        if (!row.TryGetPropertyValue(_nameProperty, out var nameNode))
            return false;

        var name = nameNode is JsonValue v && v.TryGetValue<string>(out var s) ? s : nameNode?.ToString();
        return name is not null && _nameFilter.Contains(name);
    }

    private static bool TryReadNumber(JsonObject row, string propertyPath, out double value)
    {
        value = 0;
        var segments = propertyPath.Split('/', StringSplitOptions.RemoveEmptyEntries);
        JsonNode? current = row;
        foreach (var segment in segments)
        {
            if (current is not JsonObject obj || !obj.TryGetPropertyValue(segment, out current))
                return false;
        }

        if (current is not JsonValue val)
            return false;

        if (val.TryGetValue<int>(out var i))
        {
            value = i;
            return true;
        }

        if (val.TryGetValue<long>(out var l))
        {
            value = l;
            return true;
        }

        if (val.TryGetValue<double>(out var d))
        {
            value = d;
            return true;
        }

        if (val.TryGetValue<float>(out var f))
        {
            value = f;
            return true;
        }

        return false;
    }
}
